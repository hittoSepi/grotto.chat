/**
 * Grotto Landing Page Configuration
 *
 * DIRECTORY_URL: The base URL for the directory API.
 * When served behind a reverse proxy that maps /api/ to the directory service,
 * use window.location.origin (the default). Override only if the directory
 * runs on a different domain.
 */
window.GROTTO_CHAT_CONFIG = {
    // Uses same-origin /api/ path by default (proxied by nginx to directory service)
    // DIRECTORY_URL: 'https://chat.rausku.com'
};
