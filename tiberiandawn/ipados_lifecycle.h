#ifndef IPADOS_LIFECYCLE_H
#define IPADOS_LIFECYCLE_H

void Install_IPadOS_Lifecycle_Handler(void);
bool IPadOS_Has_Recovery_Autosave(void);
bool IPadOS_Load_Recovery_Autosave(void);
void IPadOS_Discard_Recovery_Autosaves(void);

#endif
