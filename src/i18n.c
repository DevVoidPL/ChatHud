#include "i18n.h"
#include <string.h>

typedef struct { const char *pl, *en; } Translation;

const char *chat_translate(const char *text, int polish) {
    static const Translation translations[] = {
        {"Czekam na wiadomości…", "Waiting for messages…"},
        {"Łączę z YouTube…", "Connecting to YouTube…"},
        {"Nie udało się utworzyć katalogu /tmp", "Could not create a temporary directory"},
        {"Nie udało się uruchomić yt-dlp", "Could not start yt-dlp"},
        {"yt-dlp zakończył się zanim pojawił się live chat", "yt-dlp stopped before live chat became available"},
        {"Nie mogę otworzyć live_chat z yt-dlp", "Could not open the yt-dlp live chat file"},
        {"Błąd odczytu live chat z yt-dlp", "Could not read live chat from yt-dlp"},
        {"Live chat zakończony", "Live chat ended"},
        {"Nie rozpoznaję ID filmu z podanego linku", "Could not find a video ID in this URL"},
        {"Brak youtube_api_key w ~/.config/chathud/config", "A YouTube API key is required for the API backend"},
        {"Połączono z YouTube Live Chat", "Connected to YouTube Live Chat"},
        {"Nie udało się uruchomić libcurl", "Could not initialize libcurl"},
        {"Błąd sieci przy pobieraniu wiadomości", "Network error while retrieving messages"},
        {"Nie udało się uruchomić wątku yt-dlp", "Could not start the yt-dlp worker"},
        {"Nie udało się uruchomić wątku YouTube API", "Could not start the YouTube API worker"},
        {"Łączę z Twitchem…", "Connecting to Twitch…"},
        {"Utracono połączenie z Twitchem; łączę ponownie…", "Twitch connection lost; reconnecting…"},
        {"Twitch odrzucił anonimowy odczyt czatu", "Twitch: anonymous chat access was rejected"},
        {"Nie udało się uruchomić czatu Twitch", "Could not start Twitch chat"},
        {"Błąd sieci przy pobieraniu danych live", "Network error while retrieving stream data"},
        {"Nieprawidłowa odpowiedź YouTube API", "Invalid response from the YouTube API"},
        {"Nie znaleziono live dla tego linku", "No live stream found for this URL"},
        {"Live nie jest aktywny albo czat jest wyłączony", "The stream is offline or chat is disabled"},
        {"Nieprawidłowa odpowiedź z live chat", "Invalid live chat response"},
    };
    for (size_t i = 0; i < sizeof(translations) / sizeof(translations[0]); i++) {
        if (strcmp(text, translations[i].pl) == 0 || strcmp(text, translations[i].en) == 0)
            return polish ? translations[i].pl : translations[i].en;
    }
    return text;
}
