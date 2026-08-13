#if defined(IPADOS_PORT) || defined(MACOS_PORT)

#import <Foundation/Foundation.h>
#if defined(IPADOS_PORT)
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#endif

#include "common/apple_relay_transport.h"
#include "common/ipxaddr.h"
#include "common/relay_protocol.h"
#include "common/wspudp.h"
#include "platform/apple/ipados_platform.h"

#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

enum class SelectedTransport { Local, Relay };
static SelectedTransport Selection = SelectedTransport::Local;

static NSString* RelayL(const char* key)
{
    return [NSString stringWithUTF8String:TiberianDawn_LocalizedText(key)];
}

static NSString* RelayURL()
{
#if defined(MACOS_PORT)
    const char* override_url = std::getenv("TD_RELAY_URL");
    if (override_url && override_url[0]) return @(override_url);
#endif
    return @"wss://sportaktivfitness.de/tiberian-dawn-relay";
}

struct ReceivedPacket
{
    std::uint32_t source = 0;
    std::vector<std::uint8_t> bytes;
};

@interface TiberianDawnRelaySession : NSObject <NSURLSessionWebSocketDelegate>
{
@public
    std::mutex Mutex;
    std::condition_variable Condition;
    std::deque<ReceivedPacket> Packets;
    NSURLSession* Session;
    NSURLSessionWebSocketTask* Task;
    bool Ready;
    bool Failed;
    std::uint32_t PeerID;
    std::uint32_t Sequence;
    std::size_t PendingSends;
    std::string Invite;
    std::string Error;
}
- (bool)connectAsHost:(bool)host invitation:(const std::string&)invitation;
- (void)disconnect;
- (bool)isReady;
- (void)sendPayload:(const void*)payload length:(std::size_t)length target:(std::uint32_t)target;
- (bool)popPacket:(ReceivedPacket&)packet;
@end

@implementation TiberianDawnRelaySession
- (instancetype)init
{
    self = [super init];
    if (self) {
        Session = nil;
        Task = nil;
        Ready = false;
        Failed = false;
        PeerID = 0;
        Sequence = 0;
        PendingSends = 0;
    }
    return self;
}

- (void)receiveNext
{
    NSURLSessionWebSocketTask* receivingTask = nil;
    {
        std::lock_guard<std::mutex> lock(Mutex);
        receivingTask = Task;
    }
    if (!receivingTask) return;
    __weak TiberianDawnRelaySession* weakSelf = self;
    [receivingTask receiveMessageWithCompletionHandler:^(NSURLSessionWebSocketMessage* message, NSError* error) {
        TiberianDawnRelaySession* strongSelf = weakSelf;
        if (!strongSelf) return;
        {
            std::lock_guard<std::mutex> lock(strongSelf->Mutex);
            if (strongSelf->Task != receivingTask) return;
        }
        if (error) {
            std::lock_guard<std::mutex> lock(strongSelf->Mutex);
            strongSelf->Ready = false;
            strongSelf->Failed = true;
            strongSelf->Error = error.localizedDescription.UTF8String ?: "Relay connection failed";
            strongSelf->Condition.notify_all();
            return;
        }
        if (message.type == NSURLSessionWebSocketMessageTypeString) {
            NSData* data = [message.string dataUsingEncoding:NSUTF8StringEncoding];
            NSDictionary* response = data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;
            std::lock_guard<std::mutex> lock(strongSelf->Mutex);
            if ([response[@"type"] isEqual:@"ready"]) {
                strongSelf->PeerID = [response[@"peerId"] unsignedIntValue];
                strongSelf->Invite = [response[@"invite"] UTF8String] ?: "";
                strongSelf->Ready = strongSelf->PeerID != 0;
            } else if ([response[@"type"] isEqual:@"error"]) {
                strongSelf->Ready = false;
                strongSelf->Failed = true;
                strongSelf->Error = [response[@"message"] UTF8String] ?: "Relay rejected the request";
            }
            strongSelf->Condition.notify_all();
        } else if (message.type == NSURLSessionWebSocketMessageTypeData) {
            NSData* data = message.data;
            TiberianDawnRelay::Message decoded;
            if (data && TiberianDawnRelay::Decode(static_cast<const std::uint8_t*>(data.bytes), data.length, decoded)
                    == TiberianDawnRelay::DecodeResult::Ok
                && decoded.kind == TiberianDawnRelay::MessageKind::Data
                && decoded.source_peer != 0
                && decoded.payload.size() <= WS_RECEIVE_BUFFER_LEN) {
                std::lock_guard<std::mutex> lock(strongSelf->Mutex);
                if (strongSelf->Packets.size() < 512) {
                    ReceivedPacket packet;
                    packet.source = decoded.source_peer;
                    packet.bytes.swap(decoded.payload);
                    strongSelf->Packets.push_back(std::move(packet));
                }
            }
        }
        [strongSelf receiveNext];
    }];
}

- (bool)connectAsHost:(bool)host invitation:(const std::string&)invitation
{
    [self disconnect];
    {
        std::lock_guard<std::mutex> lock(Mutex);
        Ready = false;
        Failed = false;
        PeerID = 0;
        Sequence = 0;
        PendingSends = 0;
        Invite.clear();
        Error.clear();
        Packets.clear();
    }
    NSURLSessionConfiguration* configuration = NSURLSessionConfiguration.ephemeralSessionConfiguration;
    configuration.timeoutIntervalForRequest = 15;
    configuration.timeoutIntervalForResource = 30;
    NSURLSession* newSession = [NSURLSession sessionWithConfiguration:configuration delegate:self delegateQueue:nil];
    NSURLSessionWebSocketTask* newTask = [newSession webSocketTaskWithURL:[NSURL URLWithString:RelayURL()]];
    {
        std::lock_guard<std::mutex> lock(Mutex);
        Session = newSession;
        Task = newTask;
    }
    [newTask resume];
    [self receiveNext];

    NSDictionary* request = host
        ? @{@"type": @"create", @"protocol": @(TiberianDawnRelay::ProtocolVersion), @"compatibility": @(TiberianDawnRelay::CompatibilityVersion)}
        : @{@"type": @"join", @"protocol": @(TiberianDawnRelay::ProtocolVersion), @"compatibility": @(TiberianDawnRelay::CompatibilityVersion), @"invite": @(invitation.c_str())};
    NSData* json = [NSJSONSerialization dataWithJSONObject:request options:0 error:nil];
    NSString* text = [[NSString alloc] initWithData:json encoding:NSUTF8StringEncoding];
    NSURLSessionWebSocketTask* requestTask = newTask;
    [requestTask sendMessage:[[NSURLSessionWebSocketMessage alloc] initWithString:text]
        completionHandler:^(NSError* error) {
            if (!error) return;
            std::lock_guard<std::mutex> lock(Mutex);
            if (Task != requestTask) return;
            Failed = true;
            Error = error.localizedDescription.UTF8String ?: "Relay request failed";
            Condition.notify_all();
        }];

    std::unique_lock<std::mutex> lock(Mutex);
    const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (!Ready && !Failed && Condition.wait_until(lock, deadline) != std::cv_status::timeout) {}
    if (!Ready && !Failed) {
        Failed = true;
        Error = "Relay response timed out";
    }
    const bool connected = Ready && !Failed && Task == requestTask;
    lock.unlock();
    if (!connected) [self disconnect];
    return connected;
}

- (bool)isReady
{
    std::lock_guard<std::mutex> lock(Mutex);
    return Ready && !Failed && Task != nil;
}

- (void)disconnect
{
    NSURLSessionWebSocketTask* oldTask = nil;
    NSURLSession* oldSession = nil;
    {
        std::lock_guard<std::mutex> lock(Mutex);
        oldTask = Task;
        oldSession = Session;
        Task = nil;
        Session = nil;
        Ready = false;
        PeerID = 0;
        PendingSends = 0;
        Packets.clear();
        Condition.notify_all();
    }
    if (oldTask) [oldTask cancelWithCloseCode:NSURLSessionWebSocketCloseCodeGoingAway reason:nil];
    if (oldSession) [oldSession invalidateAndCancel];
}

- (void)sendPayload:(const void*)payload length:(std::size_t)length target:(std::uint32_t)target
{
    std::uint32_t peer = 0;
    std::uint32_t sequence = 0;
    NSURLSessionWebSocketTask* task = nil;
    {
        std::lock_guard<std::mutex> lock(Mutex);
        if (!Ready || Failed || !Task || length > TiberianDawnRelay::MaximumPayloadSize) return;
        if (PendingSends >= 512) {
            Ready = false;
            Failed = true;
            Error = "Relay send queue exceeded its safety limit";
            Condition.notify_all();
            return;
        }
        peer = PeerID;
        sequence = ++Sequence;
        task = Task;
        ++PendingSends;
    }
    TiberianDawnRelay::Message message;
    message.source_peer = peer;
    message.target_peer = target;
    message.sequence = sequence;
    const std::uint8_t* begin = static_cast<const std::uint8_t*>(payload);
    message.payload.assign(begin, begin + length);
    std::vector<std::uint8_t> wire;
    if (!TiberianDawnRelay::Encode(message, wire)) {
        std::lock_guard<std::mutex> lock(Mutex);
        if (Task == task && PendingSends > 0) --PendingSends;
        return;
    }
    NSData* data = [NSData dataWithBytes:wire.data() length:wire.size()];
    [task sendMessage:[[NSURLSessionWebSocketMessage alloc] initWithData:data] completionHandler:^(NSError* error) {
        std::lock_guard<std::mutex> lock(Mutex);
        if (Task != task) return;
        if (PendingSends > 0) --PendingSends;
        if (!error) return;
        Ready = false;
        Failed = true;
        Error = error.localizedDescription.UTF8String ?: "Relay send failed";
        Condition.notify_all();
    }];
}

- (void)URLSession:(NSURLSession*)session webSocketTask:(NSURLSessionWebSocketTask*)webSocketTask
    didCloseWithCode:(NSURLSessionWebSocketCloseCode)closeCode reason:(NSData*)reason
{
    (void)session;
    (void)closeCode;
    std::lock_guard<std::mutex> lock(Mutex);
    if (Task != webSocketTask) return;
    Ready = false;
    Failed = true;
    if (Error.empty()) {
        NSString* text = reason.length ? [[NSString alloc] initWithData:reason encoding:NSUTF8StringEncoding] : nil;
        Error = text.UTF8String ?: "Relay connection closed";
    }
    Condition.notify_all();
}

- (bool)popPacket:(ReceivedPacket&)packet
{
    std::lock_guard<std::mutex> lock(Mutex);
    if (Packets.empty()) return false;
    packet = std::move(Packets.front());
    Packets.pop_front();
    return true;
}
@end

static TiberianDawnRelaySession* RelaySession()
{
    static TiberianDawnRelaySession* session = [TiberianDawnRelaySession new];
    return session;
}

class AppleRelayInterfaceClass final : public WinsockInterfaceClass
{
public:
    AppleRelayInterfaceClass() { Init(); }
    ~AppleRelayInterfaceClass() override { Close_Socket(); }

    bool Open_Socket(SOCKET) override
    {
        WinsockInitialised = true;
        return [RelaySession() isReady];
    }
    void Close_Socket() override { [RelaySession() disconnect]; }
    bool Start_Listening() override { return [RelaySession() isReady]; }
    void Stop_Listening() override {}
    ProtocolEnum Get_Protocol() override { return PROTOCOL_UDP; }
    int Protocol_Event_Message() override { return WM_UDPASYNCEVENT; }

    int Message_Handler() override
    {
        ReceivedPacket received;
        while ([RelaySession() popPacket:received]) {
            WinsockBufferType* packet = new WinsockBufferType;
            if (received.bytes.size() > sizeof(packet->Buffer)) { delete packet; continue; }
            packet->BufferLen = static_cast<int>(received.bytes.size());
            packet->IsBroadcast = false;
            std::memcpy(packet->Buffer, received.bytes.data(), received.bytes.size());
            unsigned char network[4] = {0, 0, 0, 0};
            unsigned char node[6] = {
                static_cast<unsigned char>(received.source >> 24),
                static_cast<unsigned char>(received.source >> 16),
                static_cast<unsigned char>(received.source >> 8),
                static_cast<unsigned char>(received.source), 0, 0};
            IPXAddressClass address(network, node);
            std::memset(packet->Address, 0, sizeof(packet->Address));
            std::memcpy(packet->Address, &address, sizeof(address));
            if (InBuffers.Count() < 512) InBuffers.Add(packet); else delete packet;
        }
        while (OutBuffers.Count() > 0) {
            WinsockBufferType* packet = OutBuffers[0];
            std::uint32_t target = 0;
            if (!packet->IsBroadcast) {
                unsigned char network[4];
                unsigned char node[6];
                reinterpret_cast<IPXAddressClass*>(packet->Address)->Get_Address(network, node);
                target = (static_cast<std::uint32_t>(node[0]) << 24)
                    | (static_cast<std::uint32_t>(node[1]) << 16)
                    | (static_cast<std::uint32_t>(node[2]) << 8)
                    | static_cast<std::uint32_t>(node[3]);
            }
            [RelaySession() sendPayload:packet->Buffer length:packet->BufferLen target:target];
            OutBuffers.Delete(0);
            delete packet;
        }
        return 0;
    }
};

static bool ConnectRelay(bool host, const std::string& invite)
{
    return [RelaySession() connectAsHost:host invitation:invite];
}

static std::string CurrentInvite()
{
    std::lock_guard<std::mutex> lock(RelaySession()->Mutex);
    return RelaySession()->Invite;
}

static std::string CurrentError()
{
    std::lock_guard<std::mutex> lock(RelaySession()->Mutex);
    return RelaySession()->Error;
}

#if defined(MACOS_PORT)
static int SelectOnMainThread()
{
    NSAlert* choice = [NSAlert new];
    choice.messageText = RelayL("multiplayer_title");
    choice.informativeText = RelayL("multiplayer_explanation");
    [choice addButtonWithTitle:RelayL("multiplayer_local")];
    [choice addButtonWithTitle:RelayL("multiplayer_create")];
    [choice addButtonWithTitle:RelayL("multiplayer_join")];
    [choice addButtonWithTitle:RelayL("multiplayer_skirmish")];
    [choice addButtonWithTitle:RelayL("cancel")];
    const NSModalResponse response = [choice runModal];
    if (response == NSAlertFirstButtonReturn) { Selection = SelectedTransport::Local; return TIBERIAN_MULTIPLAYER_NETWORK; }
    if (response == NSAlertThirdButtonReturn + 1) return TIBERIAN_MULTIPLAYER_SKIRMISH;
    if (response != NSAlertSecondButtonReturn && response != NSAlertThirdButtonReturn) return TIBERIAN_MULTIPLAYER_CANCEL;

    bool host = response == NSAlertSecondButtonReturn;
    std::string invitation;
    if (!host) {
        NSAlert* join = [NSAlert new];
        join.messageText = RelayL("multiplayer_join");
        join.informativeText = RelayL("multiplayer_invite_prompt");
        NSTextField* field = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 360, 24)];
        field.placeholderString = @"ABC234-…";
        join.accessoryView = field;
        [join addButtonWithTitle:RelayL("multiplayer_connect")];
        [join addButtonWithTitle:RelayL("cancel")];
        if ([join runModal] != NSAlertFirstButtonReturn) return TIBERIAN_MULTIPLAYER_CANCEL;
        invitation = field.stringValue.UTF8String ?: "";
    }
    if (!ConnectRelay(host, invitation)) {
        NSAlert* error = [NSAlert new];
        error.alertStyle = NSAlertStyleCritical;
        error.messageText = RelayL("multiplayer_connection_failed");
        error.informativeText = @(CurrentError().c_str());
        [error runModal];
        return TIBERIAN_MULTIPLAYER_CANCEL;
    }
    if (host) {
        NSString* invite = @(CurrentInvite().c_str());
        [NSPasteboard.generalPasteboard clearContents];
        [NSPasteboard.generalPasteboard setString:invite forType:NSPasteboardTypeString];
        NSAlert* created = [NSAlert new];
        created.messageText = RelayL("multiplayer_room_ready");
        created.informativeText = [NSString stringWithFormat:RelayL("multiplayer_room_ready_format"), invite];
        [created addButtonWithTitle:RelayL("continue")];
        [created runModal];
    }
    Selection = SelectedTransport::Relay;
    return TIBERIAN_MULTIPLAYER_NETWORK;
}
#else
UIViewController* HostController()
{
    UIWindowScene* scene = nil;
    for (UIScene* candidate in UIApplication.sharedApplication.connectedScenes) {
        if ([candidate isKindOfClass:UIWindowScene.class] && candidate.activationState != UISceneActivationStateUnattached) {
            scene = (UIWindowScene*)candidate; break;
        }
    }
    UIViewController* controller = scene.windows.firstObject.rootViewController;
    while (controller.presentedViewController) controller = controller.presentedViewController;
    return controller;
}

static int SelectOnMainThread()
{
    __block bool finished = false;
    __block int result = TIBERIAN_MULTIPLAYER_CANCEL;
    __block bool hostRelay = false;
    __block bool joinRelay = false;
    __block std::string joinInvite;
    UIViewController* controller = HostController();
    if (!controller) return TIBERIAN_MULTIPLAYER_CANCEL;
    UIAlertController* choice = [UIAlertController alertControllerWithTitle:RelayL("multiplayer_title")
        message:RelayL("multiplayer_explanation") preferredStyle:UIAlertControllerStyleAlert];
    void (^complete)(int) = ^(int value) { result = value; finished = true; };
    [choice addAction:[UIAlertAction actionWithTitle:RelayL("multiplayer_local") style:UIAlertActionStyleDefault handler:^(UIAlertAction*) {
        Selection = SelectedTransport::Local; complete(TIBERIAN_MULTIPLAYER_NETWORK);
    }]];
    [choice addAction:[UIAlertAction actionWithTitle:RelayL("multiplayer_create") style:UIAlertActionStyleDefault handler:^(UIAlertAction*) {
        hostRelay = true; finished = true;
    }]];
    [choice addAction:[UIAlertAction actionWithTitle:RelayL("multiplayer_join") style:UIAlertActionStyleDefault handler:^(UIAlertAction*) {
        joinRelay = true; finished = true;
    }]];
    [choice addAction:[UIAlertAction actionWithTitle:RelayL("multiplayer_skirmish") style:UIAlertActionStyleDefault handler:^(UIAlertAction*) { complete(TIBERIAN_MULTIPLAYER_SKIRMISH); }]];
    [choice addAction:[UIAlertAction actionWithTitle:RelayL("cancel") style:UIAlertActionStyleCancel handler:^(UIAlertAction*) { complete(TIBERIAN_MULTIPLAYER_CANCEL); }]];
    [controller presentViewController:choice animated:YES completion:nil];
    while (!finished) [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];

    if (joinRelay) {
        [choice dismissViewControllerAnimated:NO completion:nil];
        finished = false;
        UIAlertController* join = [UIAlertController alertControllerWithTitle:RelayL("multiplayer_join")
            message:RelayL("multiplayer_invite_prompt") preferredStyle:UIAlertControllerStyleAlert];
        [join addTextFieldWithConfigurationHandler:^(UITextField* field) { field.placeholder = @"ABC234-…"; field.autocapitalizationType = UITextAutocapitalizationTypeAllCharacters; }];
        [join addAction:[UIAlertAction actionWithTitle:RelayL("cancel") style:UIAlertActionStyleCancel handler:^(UIAlertAction*) { complete(TIBERIAN_MULTIPLAYER_CANCEL); }]];
        [join addAction:[UIAlertAction actionWithTitle:RelayL("multiplayer_connect") style:UIAlertActionStyleDefault handler:^(UIAlertAction*) {
            joinInvite = join.textFields.firstObject.text.UTF8String ?: ""; finished = true;
        }]];
        [controller presentViewController:join animated:YES completion:nil];
        while (!finished) [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
    }
    if (result != TIBERIAN_MULTIPLAYER_CANCEL || (!hostRelay && joinInvite.empty())) return result;

    if (!ConnectRelay(hostRelay, joinInvite)) {
        __block bool acknowledged = false;
        UIAlertController* error = [UIAlertController alertControllerWithTitle:RelayL("multiplayer_connection_failed")
            message:@(CurrentError().c_str()) preferredStyle:UIAlertControllerStyleAlert];
        [error addAction:[UIAlertAction actionWithTitle:RelayL("ok") style:UIAlertActionStyleDefault handler:^(UIAlertAction*) { acknowledged = true; }]];
        [controller presentViewController:error animated:YES completion:nil];
        while (!acknowledged) [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
        return TIBERIAN_MULTIPLAYER_CANCEL;
    }
    if (hostRelay) {
        NSString* invite = @(CurrentInvite().c_str());
        UIPasteboard.generalPasteboard.string = invite;
        __block bool acknowledged = false;
        UIAlertController* created = [UIAlertController alertControllerWithTitle:RelayL("multiplayer_room_ready")
            message:[NSString stringWithFormat:RelayL("multiplayer_room_ready_format"), invite] preferredStyle:UIAlertControllerStyleAlert];
        [created addAction:[UIAlertAction actionWithTitle:RelayL("continue") style:UIAlertActionStyleDefault handler:^(UIAlertAction*) { acknowledged = true; }]];
        [controller presentViewController:created animated:YES completion:nil];
        while (!acknowledged) [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
    }
    Selection = SelectedTransport::Relay;
    return TIBERIAN_MULTIPLAYER_NETWORK;
}
#endif
int TiberianDawn_SelectMultiplayerTransport(void)
{
    __block int result = TIBERIAN_MULTIPLAYER_CANCEL;
    void (^work)(void) = ^{ result = SelectOnMainThread(); };
    if (NSThread.isMainThread) work(); else dispatch_sync(dispatch_get_main_queue(), work);
    return result;
}

WinsockInterfaceClass* TiberianDawn_CreateNetworkTransport(void)
{
    return Selection == SelectedTransport::Relay
        ? static_cast<WinsockInterfaceClass*>(new AppleRelayInterfaceClass)
        : static_cast<WinsockInterfaceClass*>(new UDPInterfaceClass);
}

void TiberianDawn_ResetNetworkTransport(void)
{
    [RelaySession() disconnect];
    Selection = SelectedTransport::Local;
}

#endif
