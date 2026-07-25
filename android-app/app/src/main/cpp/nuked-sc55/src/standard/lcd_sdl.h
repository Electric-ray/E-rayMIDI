#pragma once

#include "lcd.h"
#include <SDL.h>

class LCD_SDL_Backend : public LCD_Backend
{
public:
    LCD_SDL_Backend() = default;
    ~LCD_SDL_Backend();

    bool Start(const lcd_t& lcd);
    void Stop();

    void HandleEvent(const SDL_Event& ev);
    void Render();

    bool IsQuitRequested() const;

private:
    const lcd_t*  m_lcd      = nullptr;
    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    SDL_Texture*  m_texture  = nullptr;

    // When the user closes the window this becomes true
    bool m_quit_requested = false;
};
