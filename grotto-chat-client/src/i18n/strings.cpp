#include "i18n/strings.hpp"
#include <unordered_map>
#include <sstream>

namespace grotto::i18n {

namespace {

std::string g_language = "en";

const std::unordered_map<I18nKey, std::string> kEnglish = {
    // Login screen / init
    {I18nKey::FAILED_OPEN_DATA_STORE, "Failed to open local data store: {0}"},
    {I18nKey::FAILED_UNLOCK_IDENTITY, "Failed to unlock local identity. Use the original passkey or press CLEAR CREDS."},
    {I18nKey::CLEAR_LOCAL_DATA_SUCCESS, "Local credentials and identity data cleared. Enter the same username and passkey to re-sync keys."},
    {I18nKey::CLEAR_LOCAL_DATA_NOT_FOUND, "No local credential files were found. Enter the same username and passkey to re-sync keys."},
    {I18nKey::FAILED_CLEAR_LOCAL_DATA, "Failed to clear local data at {0}: {1}"},
    {I18nKey::HOST_LABEL, "HOST:     "},
    {I18nKey::PORT_LABEL, "PORT:     "},
    {I18nKey::USERNAME_LABEL, "USERNAME: "},
    {I18nKey::PASSKEY_LABEL, "PASSKEY:  "},
    {I18nKey::REMEMBER_CREDENTIALS, " Remember credentials"},
    {I18nKey::BUTTON_CONNECT, "[ CONNECT ]"},
    {I18nKey::BUTTON_CLEAR_CREDS, "[ CLEAR CREDS ]"},
    {I18nKey::BUTTON_QUIT, "[ QUIT ]"},
    {I18nKey::HOST_REQUIRED, "Host is required"},
    {I18nKey::PORT_REQUIRED, "Port is required"},
    {I18nKey::PORT_RANGE_ERROR, "Port must be between 1 and 65535"},
    {I18nKey::PORT_NUMBER_ERROR, "Port must be a valid number"},
    {I18nKey::USERNAME_REQUIRED, "Username is required"},
    {I18nKey::PASSKEY_REQUIRED, "Passkey is required"},
    {I18nKey::CONNECTING, "Connecting..."},

    // Settings categories
    {I18nKey::CATEGORY_GENERAL, "General"},
    {I18nKey::CATEGORY_APPEARANCE, "Appearance"},
    {I18nKey::CATEGORY_CONNECTION, "Connection"},
    {I18nKey::CATEGORY_NOTIFICATIONS, "Notifications"},
    {I18nKey::CATEGORY_ACCOUNT, "Account"},

    // Settings labels
    {I18nKey::THEME_LABEL, "Theme: "},
    {I18nKey::THEME_AVAILABLE, " available"},
    {I18nKey::FONT_SCALE_LABEL, "Font Scale: "},
    {I18nKey::TIMESTAMP_FORMAT_LABEL, "Timestamp Format: "},
    {I18nKey::MAX_MESSAGES_LABEL, "Max Messages: "},
    {I18nKey::THEME_SETTINGS, "Theme Settings"},
    {I18nKey::DISPLAY_OPTIONS, "Display Options"},
    {I18nKey::THEME_NOTE, "Note: Theme changes apply immediately. Other changes apply on save."},
    {I18nKey::CLIPBOARD_SETTINGS, "Clipboard"},
    {I18nKey::COPY_ON_RELEASE, "Copy selection on mouse release"},
    {I18nKey::PREVIEW_SETTINGS, "Image Preview"},
    {I18nKey::INLINE_IMAGES, "Show inline images"},
    {I18nKey::IMAGE_COLUMNS_LABEL, "Preview Width (cols): "},
    {I18nKey::IMAGE_ROWS_LABEL, "Preview Height (rows): "},
    {I18nKey::TERMINAL_GRAPHICS_LABEL, "Terminal Graphics: "},
    {I18nKey::TERMINAL_GRAPHICS_HINT, "  Allowed: auto, off, viewer-only"},
    {I18nKey::RECONNECT_DELAY_LABEL, "Reconnect Delay: "},
    {I18nKey::CONNECTION_TIMEOUT_LABEL, "Connection Timeout: "},
    {I18nKey::CERT_PIN_LABEL, "Certificate PIN: "},
    {I18nKey::RECONNECT_BEHAVIOR, "Reconnect Behavior"},
    {I18nKey::TIMEOUT_SETTINGS, "Timeout Settings"},
    {I18nKey::TLS_OPTIONS, "TLS/SSL Options"},
    {I18nKey::CONNECTION_NOTE, "Note: Connection settings take effect on next connection."},
    {I18nKey::MENTION_KEYWORDS_LABEL, "Mention Keywords: "},
    {I18nKey::NOTIFICATION_SETTINGS, "Notification Settings"},
    {I18nKey::MENTION_SETTINGS, "Mention Settings"},
    {I18nKey::MENTION_KEYWORDS_HINT, "  Comma-separated keywords (e.g.: nick1, nick2, word1)"},
    {I18nKey::NICKNAME_LABEL, "Nickname: "},
    {I18nKey::NICKNAME_HINT, "  This nickname will be used for new connections."},
    {I18nKey::PUBLIC_KEY_LABEL, "Public Key (Ed25519):"},
    {I18nKey::PUBLIC_KEY_NOT_AVAILABLE, "(not available)"},
    {I18nKey::DANGER_ZONE, "Danger Zone"},
    {I18nKey::ACCOUNT_SETTINGS, "Account Settings"},
    {I18nKey::IMPORT_EXPORT, "Import/Export"},
    {I18nKey::LANGUAGE_LABEL, "Language: "},

    // Settings checkboxes
    {I18nKey::SHOW_TIMESTAMPS, "Show timestamps"},
    {I18nKey::COLORIZE_USERNAMES, "Colorize usernames"},
    {I18nKey::AUTO_RECONNECT, "Auto-reconnect on disconnect"},
    {I18nKey::VERIFY_TLS, "Verify TLS certificates"},
    {I18nKey::DESKTOP_NOTIFICATIONS, "Enable desktop notifications"},
    {I18nKey::SOUND_ALERTS, "Enable sound alerts"},
    {I18nKey::NOTIFY_ON_MENTION, "Notify on mentions"},
    {I18nKey::NOTIFY_ON_DM, "Notify on direct messages"},

    // Settings buttons
    {I18nKey::BUTTON_SAVE, "[ SAVE ]"},
    {I18nKey::BUTTON_CANCEL, "[ CANCEL ]"},
    {I18nKey::BUTTON_RESET_DEFAULTS, "[ Reset to Defaults ]"},
    {I18nKey::BUTTON_EXPORT_SETTINGS, "[ Export Settings ]"},
    {I18nKey::BUTTON_IMPORT_SETTINGS, "[ Import Settings ]"},

    // App / system messages
    {I18nKey::GROTTO_CONNECTING, "Grotto v{0} — connecting to {1}:{2}"},
    {I18nKey::UNKNOWN_COMMAND, "Unknown command: {0}"},
    {I18nKey::DISCONNECTED_FROM_SERVER, "Disconnected from server."},
    {I18nKey::NOT_CONNECTED, "Not connected."},
    {I18nKey::CLIENT_VERSION, "Client version: {0}"},
    {I18nKey::SERVER_VERSION, "Server version: {0}"},
    {I18nKey::CONNECTION_STATUS, "Connection: {0}"},
    {I18nKey::AUTH_STATUS, "Auth: {0}"},
    {I18nKey::USER_LABEL, "User: {0}"},
    {I18nKey::ACTIVE_CHANNEL, "Active channel: {0}"},
    {I18nKey::NONE, "(none)"},
    {I18nKey::HELP_USAGE, "Usage: /help <topic>"},
    {I18nKey::AVAILABLE_TOPICS, "Available topics: {0}"},
    {I18nKey::TOPIC_NOT_FOUND, "Topic '{0}' not found."},
    {I18nKey::HELP_RELOADED, "Help files reloaded."},
    {I18nKey::HELP_NOT_INITIALIZED, "Help system not initialized."},
    {I18nKey::CLEARED, "(cleared)"},
    {I18nKey::ONLINE_USERS, "Online: {0}"},
    {I18nKey::SEARCH_RESULTS, "Search results for '{0}':"},
    {I18nKey::NO_RESULTS, "  (no results)"},
    {I18nKey::SEARCH_NOT_AVAILABLE, "Search not available (no database)."},
    {I18nKey::NOT_CONNECTED_CHECK_STATUS, "Not connected. Use /status to check connection state."},
    {I18nKey::CANNOT_LEAVE_SERVER_CHANNEL, "Cannot leave the server channel."},
    {I18nKey::INVALID_CHANNEL_NAME, "Invalid channel name. Use letters, numbers, ., - or _."},
    {I18nKey::SERVER_RESERVED_TAB, "`server` is a reserved internal tab."},
    {I18nKey::CALLING, "Calling {0}..."},
    {I18nKey::ACCEPTED_CALL, "Accepted call from {0}"},
    {I18nKey::CALL_ENDED, "Call ended."},
    {I18nKey::LEFT_VOICE_ROOM, "Left voice room."},
    {I18nKey::JOINING_VOICE_ROOM, "Joining voice room: {0}..."},
    {I18nKey::MUTED, "Muted."},
    {I18nKey::UNMUTED, "Unmuted."},
    {I18nKey::DEAFENED, "Deafened."},
    {I18nKey::UNDEAFENED, "Undeafened."},
    {I18nKey::VOICE_MODE, "Voice mode: {0}"},
    {I18nKey::FILE_NOT_FOUND, "File not found: {0}"},
    {I18nKey::FILE_TRANSFER_NOT_AVAILABLE, "File transfer not available."},
    {I18nKey::UPLOAD_FAILED, "Upload failed to start."},
    {I18nKey::UPLOADING, "Uploading {0} ({1} bytes)..."},
    {I18nKey::DOWNLOAD_FAILED, "Download failed to start."},
    {I18nKey::DOWNLOADING, "Downloading {0} to {1}..."},
    {I18nKey::SAFETY_NUMBER_WITH, "Safety number with {0}:"},
    {I18nKey::NO_ACTIVE_CHANNEL, "No active channel."},
    {I18nKey::CANNOT_SEND_MESSAGES_HERE, "Cannot send messages here. Use /join #channel or /msg <user>."},
    {I18nKey::MESSAGE_TOO_LONG, "Message is too long ({0} bytes, max {1})."},
    {I18nKey::SWITCHED_TO, "Switched to {0}"},
    {I18nKey::SETTINGS_SAVED, "Settings saved."},
    {I18nKey::SETTINGS_CANCELLED, "Settings cancelled."},
    {I18nKey::LOGGING_OUT, "Logging out..."},

    // Message handler
    {I18nKey::AUTHENTICATED_AS, "Authenticated as {0}"},
    {I18nKey::AUTH_FAILED, "Authentication failed: {0}"},
    {I18nKey::AUTH_FAILED_CHECK_KEY, "Authentication failed! Check your identity key."},
    {I18nKey::FAILED_ESTABLISH_SESSION, "Failed to establish session with {0} (key mismatch?)"},
    {I18nKey::FAILED_SEND_MESSAGES, "Failed to send {0} message(s) to {1}"},
    {I18nKey::COULD_NOT_ESTABLISH_SESSION, "Could not establish session with {0}"},
    {I18nKey::USER_NOT_FOUND, "User not found: {0}"},
    {I18nKey::SERVER_ERROR, "Server error: {0}"},
    {I18nKey::COMMAND_RESPONSE, "Command {0}: {1}"},
    {I18nKey::FILE_TRANSFER_COMPLETED, "File transfer completed: {0}"},
    {I18nKey::FILE_TRANSFER_ERROR, "File transfer error: {0}"},

    // Status bar
    {I18nKey::USERS_COUNT, "{0} users"},
    {I18nKey::MUTED_INDICATOR, "[MUTED]"},
    {I18nKey::DEAFENED_INDICATOR, "[DEAF]"},
    {I18nKey::PTT_F1, "PTT: F1"},
    {I18nKey::VOX, "VOX"},

    // Message view
    {I18nKey::NO_MESSAGES, "(no messages)"},

    // User list panel
    {I18nKey::USERS_HEADER, " USERS: "},
    {I18nKey::OFFLINE_HEADER, " OFFLINE: "},
    {I18nKey::VOICE_HEADER, " VOICE: "},

    // Voice engine
    {I18nKey::FAILED_OPEN_AUDIO_DEVICE, "Failed to open audio device. Check your audio settings."},
    {I18nKey::VOICE_CONNECTION_FAILED, "Voice connection to {0} failed. Check your network/firewall."},

    // Status values
    {I18nKey::CONNECTED, "connected"},
    {I18nKey::DISCONNECTED, "disconnected"},
    {I18nKey::AUTHENTICATED, "authenticated"},
    {I18nKey::NOT_AUTHENTICATED, "not authenticated"},

    // Misc
    {I18nKey::BUTTON_LOGOUT, "[ LOGOUT ]"},
    {I18nKey::SECONDS, " seconds"},
};

const std::unordered_map<I18nKey, std::string> kFinnish = {
    // Login screen / init
    {I18nKey::FAILED_OPEN_DATA_STORE, "Paikallisen tietokannan avaaminen epäonnistui: {0}"},
    {I18nKey::FAILED_UNLOCK_IDENTITY, "Paikallisen identiteetin avaaminen epäonnistui. Käytä alkuperäistä salasanaa tai paina TYHJENNÄ."},
    {I18nKey::CLEAR_LOCAL_DATA_SUCCESS, "Paikalliset tunnukset ja identiteettidata tyhjennetty. Syötä sama käyttäjänimi ja salasana avainten uudelleensynkronointiin."},
    {I18nKey::CLEAR_LOCAL_DATA_NOT_FOUND, "Paikallisia tunnustiedostoja ei löytynyt. Syötä sama käyttäjänimi ja salasana avainten uudelleensynkronointiin."},
    {I18nKey::FAILED_CLEAR_LOCAL_DATA, "Paikallisen datan tyhjennys epäonnistui kohteessa {0}: {1}"},
    {I18nKey::HOST_LABEL, "PALVELIN: "},
    {I18nKey::PORT_LABEL, "PORTTI:   "},
    {I18nKey::USERNAME_LABEL, "KÄYTTÄJÄ: "},
    {I18nKey::PASSKEY_LABEL, "SALASANA: "},
    {I18nKey::REMEMBER_CREDENTIALS, " Muista tunnukset"},
    {I18nKey::BUTTON_CONNECT, "[ YHDISTÄ ]"},
    {I18nKey::BUTTON_CLEAR_CREDS, "[ TYHJENNÄ ]"},
    {I18nKey::BUTTON_QUIT, "[ POISTU ]"},
    {I18nKey::HOST_REQUIRED, "Palvelin vaaditaan"},
    {I18nKey::PORT_REQUIRED, "Portti vaaditaan"},
    {I18nKey::PORT_RANGE_ERROR, "Portin oltava välillä 1–65535"},
    {I18nKey::PORT_NUMBER_ERROR, "Portin oltava kelvollinen numero"},
    {I18nKey::USERNAME_REQUIRED, "Käyttäjä vaaditaan"},
    {I18nKey::PASSKEY_REQUIRED, "Salasana vaaditaan"},
    {I18nKey::CONNECTING, "Yhdistetään..."},

    // Settings categories
    {I18nKey::CATEGORY_GENERAL, "Yleiset"},
    {I18nKey::CATEGORY_APPEARANCE, "Ulkoasu"},
    {I18nKey::CATEGORY_CONNECTION, "Yhteys"},
    {I18nKey::CATEGORY_NOTIFICATIONS, "Ilmoitukset"},
    {I18nKey::CATEGORY_ACCOUNT, "Tili"},

    // Settings labels
    {I18nKey::THEME_LABEL, "Teema: "},
    {I18nKey::THEME_AVAILABLE, " saatavilla"},
    {I18nKey::FONT_SCALE_LABEL, "Fonttikoko: "},
    {I18nKey::TIMESTAMP_FORMAT_LABEL, "Aikaleiman muoto: "},
    {I18nKey::MAX_MESSAGES_LABEL, "Viestien enimmäismäärä: "},
    {I18nKey::THEME_SETTINGS, "Teema-asetukset"},
    {I18nKey::DISPLAY_OPTIONS, "Näyttöasetukset"},
    {I18nKey::THEME_NOTE, "Huom: Teeman muutokset tulevat voimaan heti. Muut tallennuksen yhteydessä."},
    {I18nKey::CLIPBOARD_SETTINGS, "Leikepöytä"},
    {I18nKey::COPY_ON_RELEASE, "Kopioi valinta hiiren vapautuksella"},
    {I18nKey::PREVIEW_SETTINGS, "Kuvien esikatselu"},
    {I18nKey::INLINE_IMAGES, "Näytä kuvat viestivirrassa"},
    {I18nKey::IMAGE_COLUMNS_LABEL, "Esikatselun leveys (sar): "},
    {I18nKey::IMAGE_ROWS_LABEL, "Esikatselun korkeus (riv): "},
    {I18nKey::TERMINAL_GRAPHICS_LABEL, "Terminaaligrafiikka: "},
    {I18nKey::TERMINAL_GRAPHICS_HINT, "  Sallitut: auto, off, viewer-only"},
    {I18nKey::RECONNECT_DELAY_LABEL, "Uudelleenyhdistysviive: "},
    {I18nKey::CONNECTION_TIMEOUT_LABEL, "Yhteyden aikakatkaisu: "},
    {I18nKey::CERT_PIN_LABEL, "Varmenteen PIN: "},
    {I18nKey::RECONNECT_BEHAVIOR, "Uudelleenyhdistäminen"},
    {I18nKey::TIMEOUT_SETTINGS, "Aikakatkaisuasetukset"},
    {I18nKey::TLS_OPTIONS, "TLS/SSL-asetukset"},
    {I18nKey::CONNECTION_NOTE, "Huom: Yhteysasetukset tulevat voimaan seuraavalla yhteydellä."},
    {I18nKey::MENTION_KEYWORDS_LABEL, "Mainintasanat: "},
    {I18nKey::NOTIFICATION_SETTINGS, "Ilmoitusasetukset"},
    {I18nKey::MENTION_SETTINGS, "Maininta-asetukset"},
    {I18nKey::MENTION_KEYWORDS_HINT, "  Pilkulla erotetut sanat (esim.: nim1, nim2, sana1)"},
    {I18nKey::NICKNAME_LABEL, "Nimimerkki: "},
    {I18nKey::NICKNAME_HINT, "  Tätä nimimerkkiä käytetään uusissa yhteyksissä."},
    {I18nKey::PUBLIC_KEY_LABEL, "Julkinen avain (Ed25519):"},
    {I18nKey::PUBLIC_KEY_NOT_AVAILABLE, "(ei saatavilla)"},
    {I18nKey::DANGER_ZONE, "Vaaravyöhyke"},
    {I18nKey::ACCOUNT_SETTINGS, "Tiliasetukset"},
    {I18nKey::IMPORT_EXPORT, "Tuonti/Vienti"},
    {I18nKey::LANGUAGE_LABEL, "Kieli: "},

    // Settings checkboxes
    {I18nKey::SHOW_TIMESTAMPS, "Näytä aikaleimat"},
    {I18nKey::COLORIZE_USERNAMES, "Väritä käyttäjänimet"},
    {I18nKey::AUTO_RECONNECT, "Yhdistä automaattisesti uudelleen"},
    {I18nKey::VERIFY_TLS, "Tarkista TLS-varmenteet"},
    {I18nKey::DESKTOP_NOTIFICATIONS, "Ota työpöytäilmoitukset käyttöön"},
    {I18nKey::SOUND_ALERTS, "Ota äänihälytykset käyttöön"},
    {I18nKey::NOTIFY_ON_MENTION, "Ilmoita maininnoista"},
    {I18nKey::NOTIFY_ON_DM, "Ilmoita yksityisviesteistä"},

    // Settings buttons
    {I18nKey::BUTTON_SAVE, "[ TALLENNA ]"},
    {I18nKey::BUTTON_CANCEL, "[ PERUUTA ]"},
    {I18nKey::BUTTON_RESET_DEFAULTS, "[ Palauta oletukset ]"},
    {I18nKey::BUTTON_EXPORT_SETTINGS, "[ Vie asetukset ]"},
    {I18nKey::BUTTON_IMPORT_SETTINGS, "[ Tuo asetukset ]"},

    // App / system messages
    {I18nKey::GROTTO_CONNECTING, "Grotto v{0} — yhdistetään kohteeseen {1}:{2}"},
    {I18nKey::UNKNOWN_COMMAND, "Tuntematon komento: {0}"},
    {I18nKey::DISCONNECTED_FROM_SERVER, "Yhteys palvelimeen katkaistu."},
    {I18nKey::NOT_CONNECTED, "Ei yhteyttä."},
    {I18nKey::CLIENT_VERSION, "Clientin versio: {0}"},
    {I18nKey::SERVER_VERSION, "Serverin versio: {0}"},
    {I18nKey::CONNECTION_STATUS, "Yhteys: {0}"},
    {I18nKey::AUTH_STATUS, "Tunnistus: {0}"},
    {I18nKey::USER_LABEL, "Käyttäjä: {0}"},
    {I18nKey::ACTIVE_CHANNEL, "Aktiivinen kanava: {0}"},
    {I18nKey::NONE, "(ei mitään)"},
    {I18nKey::HELP_USAGE, "Käyttö: /help <aihe>"},
    {I18nKey::AVAILABLE_TOPICS, "Saatavilla olevat aiheet: {0}"},
    {I18nKey::TOPIC_NOT_FOUND, "Aihetta '{0}' ei löytynyt."},
    {I18nKey::HELP_RELOADED, "Ohjetiedostot ladattu uudelleen."},
    {I18nKey::HELP_NOT_INITIALIZED, "Ohjesysteemiä ei ole alustettu."},
    {I18nKey::CLEARED, "(tyhjennetty)"},
    {I18nKey::ONLINE_USERS, "Paikalla: {0}"},
    {I18nKey::SEARCH_RESULTS, "Hakutulokset haulle '{0}':"},
    {I18nKey::NO_RESULTS, "  (ei tuloksia)"},
    {I18nKey::SEARCH_NOT_AVAILABLE, "Haku ei käytettävissä (ei tietokantaa)."},
    {I18nKey::NOT_CONNECTED_CHECK_STATUS, "Ei yhteyttä. Tarkista tila komennolla /status."},
    {I18nKey::CANNOT_LEAVE_SERVER_CHANNEL, "Serverikanavaa ei voi poistua."},
    {I18nKey::INVALID_CHANNEL_NAME, "Virheellinen kanavan nimi. Käytä kirjaimia, numeroita, ., - tai _."},
    {I18nKey::SERVER_RESERVED_TAB, "`server` on varattu sisäinen välilehti."},
    {I18nKey::CALLING, "Soitetaan käyttäjälle {0}..."},
    {I18nKey::ACCEPTED_CALL, "Vastattu puheluun käyttäjältä {0}"},
    {I18nKey::CALL_ENDED, "Puhelu päättyi."},
    {I18nKey::LEFT_VOICE_ROOM, "Poistuttu äänihuoneesta."},
    {I18nKey::JOINING_VOICE_ROOM, "Liitytään äänihuoneeseen: {0}..."},
    {I18nKey::MUTED, "Mykistetty."},
    {I18nKey::UNMUTED, "Mykistys poistettu."},
    {I18nKey::DEAFENED, "Äänet mykistetty."},
    {I18nKey::UNDEAFENED, "Äänien mykistys poistettu."},
    {I18nKey::VOICE_MODE, "Äänitila: {0}"},
    {I18nKey::FILE_NOT_FOUND, "Tiedostoa ei löytynyt: {0}"},
    {I18nKey::FILE_TRANSFER_NOT_AVAILABLE, "Tiedostonsiirto ei käytettävissä."},
    {I18nKey::UPLOAD_FAILED, "Lähetyksen aloitus epäonnistui."},
    {I18nKey::UPLOADING, "Lähetetään {0} ({1} tavua)..."},
    {I18nKey::DOWNLOAD_FAILED, "Latauksen aloitus epäonnistui."},
    {I18nKey::DOWNLOADING, "Ladataan {0} kohteeseen {1}..."},
    {I18nKey::SAFETY_NUMBER_WITH, "Turvanumero käyttäjän {0} kanssa:"},
    {I18nKey::NO_ACTIVE_CHANNEL, "Ei aktiivista kanavaa."},
    {I18nKey::CANNOT_SEND_MESSAGES_HERE, "Tänne ei voi lähettää viestejä. Käytä /join #kanava tai /msg <käyttäjä>."},
    {I18nKey::MESSAGE_TOO_LONG, "Viesti on liian pitkä ({0} tavua, maksimi {1})."},
    {I18nKey::SWITCHED_TO, "Vaihdettu kanavalle {0}"},
    {I18nKey::SETTINGS_SAVED, "Asetukset tallennettu."},
    {I18nKey::SETTINGS_CANCELLED, "Asetukset peruutettu."},
    {I18nKey::LOGGING_OUT, "Kirjaudutaan ulos..."},

    // Message handler
    {I18nKey::AUTHENTICATED_AS, "Tunnistauduttu käyttäjänä {0}"},
    {I18nKey::AUTH_FAILED, "Tunnistautuminen epäonnistui: {0}"},
    {I18nKey::AUTH_FAILED_CHECK_KEY, "Tunnistautuminen epäonnistui! Tarkista tunnisteavain."},
    {I18nKey::FAILED_ESTABLISH_SESSION, "Istunnon muodostaminen käyttäjän {0} kanssa epäonnistui (avain ei täsmää?)"},
    {I18nKey::FAILED_SEND_MESSAGES, "{0} viestin lähetys käyttäjälle {1} epäonnistui"},
    {I18nKey::COULD_NOT_ESTABLISH_SESSION, "Istunnon muodostaminen käyttäjän {0} kanssa ei onnistunut"},
    {I18nKey::USER_NOT_FOUND, "Käyttäjää ei löytynyt: {0}"},
    {I18nKey::SERVER_ERROR, "Palvelinvirhe: {0}"},
    {I18nKey::COMMAND_RESPONSE, "Komento {0}: {1}"},
    {I18nKey::FILE_TRANSFER_COMPLETED, "Tiedostonsiirto valmis: {0}"},
    {I18nKey::FILE_TRANSFER_ERROR, "Tiedostonsiirtovirhe: {0}"},

    // Status bar
    {I18nKey::USERS_COUNT, "{0} käyttäjää"},
    {I18nKey::MUTED_INDICATOR, "[MYKISTETTY]"},
    {I18nKey::DEAFENED_INDICATOR, "[ÄÄNET POIS]"},
    {I18nKey::PTT_F1, "PTT: F1"},
    {I18nKey::VOX, "VOX"},

    // Message view
    {I18nKey::NO_MESSAGES, "(ei viestejä)"},

    // User list panel
    {I18nKey::USERS_HEADER, " KÄYTTÄJÄT: "},
    {I18nKey::OFFLINE_HEADER, " OFFLINE: "},
    {I18nKey::VOICE_HEADER, " ÄÄNI: "},

    // Voice engine
    {I18nKey::FAILED_OPEN_AUDIO_DEVICE, "Äänilaitteen avaaminen epäonnistui. Tarkista ääniasetukset."},
    {I18nKey::VOICE_CONNECTION_FAILED, "Ääniyhteys käyttäjään {0} epäonnistui. Tarkista verkko/palomuuri."},

    // Status values
    {I18nKey::CONNECTED, "yhdistetty"},
    {I18nKey::DISCONNECTED, "ei yhteyttä"},
    {I18nKey::AUTHENTICATED, "tunnistautunut"},
    {I18nKey::NOT_AUTHENTICATED, "ei tunnistautunut"},

    // Misc
    {I18nKey::BUTTON_LOGOUT, "[ KIRJAUDU ULOS ]"},
    {I18nKey::SECONDS, " sekuntia"},
};

std::string apply_placeholders(const std::string& tmpl, const std::vector<std::string>& args) {
    std::string result = tmpl;
    for (size_t i = 0; i < args.size(); ++i) {
        std::string placeholder = "{" + std::to_string(i) + "}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.size(), args[i]);
            pos += args[i].size();
        }
    }
    return result;
}

} // anonymous namespace

void set_language(const std::string& lang) {
    g_language = lang;
}

std::string current_language() {
    return g_language;
}

std::string tr(I18nKey key) {
    return tr(key, std::vector<std::string>{});
}

std::string tr(I18nKey key, const std::string& arg0) {
    return tr(key, std::vector<std::string>{arg0});
}

std::string tr(I18nKey key, const std::string& arg0, const std::string& arg1) {
    return tr(key, std::vector<std::string>{arg0, arg1});
}

std::string tr(I18nKey key, const std::string& arg0, const std::string& arg1, const std::string& arg2) {
    return tr(key, std::vector<std::string>{arg0, arg1, arg2});
}

std::string tr(I18nKey key, const std::vector<std::string>& args) {
    const auto* map = (g_language == "fi") ? &kFinnish : &kEnglish;
    auto it = map->find(key);
    if (it == map->end()) {
        // Fallback to English if key missing in selected language
        it = kEnglish.find(key);
        if (it == kEnglish.end()) {
            return "[missing translation]";
        }
    }
    if (args.empty()) {
        return it->second;
    }
    return apply_placeholders(it->second, args);
}

} // namespace grotto::i18n
