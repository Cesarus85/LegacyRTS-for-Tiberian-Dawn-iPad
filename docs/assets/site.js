(() => {
  const isGitHubPages = location.hostname.endsWith('.github.io');
  if (isGitHubPages) {
    const owner = location.hostname.slice(0, -'.github.io'.length);
    const repository = location.pathname.split('/').filter(Boolean)[0];
    if (owner && repository) {
      const repositoryRoot = `https://github.com/${owner}/${repository}`;
      document.querySelectorAll('[data-repo-root]').forEach((link) => {
        link.href = repositoryRoot;
      });
      document.querySelectorAll('[data-repo-path]').forEach((link) => {
        link.href = `${repositoryRoot}/blob/main/${link.dataset.repoPath}`;
      });
    }
  }

  const buttons = document.querySelectorAll('[data-language]');
  const translatable = document.querySelectorAll('[data-de][data-en]');

  function setLanguage(language) {
    document.documentElement.lang = language;
    translatable.forEach((element) => {
      element.textContent = element.dataset[language];
    });
    buttons.forEach((button) => {
      button.setAttribute('aria-pressed', String(button.dataset.language === language));
    });
    try {
      localStorage.setItem('tiberian-dawn-apple-language', language);
    } catch (_) {
      // The language switch works even when storage is unavailable.
    }
  }

  buttons.forEach((button) => {
    button.addEventListener('click', () => setLanguage(button.dataset.language));
  });

  let savedLanguage = null;
  try {
    savedLanguage = localStorage.getItem('tiberian-dawn-apple-language');
  } catch (_) {}
  const preferredLanguage = navigator.language.toLowerCase().startsWith('de') ? 'de' : 'en';
  setLanguage(savedLanguage === 'de' || savedLanguage === 'en' ? savedLanguage : preferredLanguage);
})();
