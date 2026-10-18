
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define SCREEN_WIDTH  1280
#define SCREEN_HEIGHT 720

static void draw_filled_circle(SDL_Renderer *renderer,
                               int cx, int cy, int radius)
{
    for (int y = -radius; y <= radius; y++) {
        int half_width = (int)sqrt(
            (double)radius * radius - (double)y * y
        );

        SDL_RenderDrawLine(renderer,
                           cx - half_width, cy + y,
                           cx + half_width, cy + y);
    }
}

int main(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "bomb_os",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        SDL_WINDOW_FULLSCREEN
    );

    if (!window) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_SOFTWARE
    );

    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool running = true;
    SDL_Event event;
    Uint32 start = SDL_GetTicks();

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;

            if (event.type == SDL_KEYDOWN &&
                event.key.keysym.sym == SDLK_ESCAPE)
                running = false;
        }

        int width, height;
        SDL_GetRendererOutputSize(renderer, &width, &height);

        int cx = width / 2;
        int cy = height / 2;

        float time = (SDL_GetTicks() - start) / 1000.0f;
        int fuse_length = 65 + (int)(5 * sinf(time * 4));

        // Dark background
        SDL_SetRenderDrawColor(renderer, 12, 12, 20, 255);
        SDL_RenderClear(renderer);

        // Bomb body
        SDL_SetRenderDrawColor(renderer, 190, 25, 40, 255);
        draw_filled_circle(renderer, cx, cy, 100);

        // Highlight
        SDL_SetRenderDrawColor(renderer, 255, 65, 75, 255);
        draw_filled_circle(renderer, cx - 28, cy - 30, 22);

        // Fuse
        SDL_SetRenderDrawColor(renderer, 220, 180, 120, 255);
        SDL_RenderDrawLine(renderer,
                           cx + 55, cy - 80,
                           cx + fuse_length, cy - 140);

        // Animated spark
        SDL_SetRenderDrawColor(renderer, 255, 190, 20, 255);
        draw_filled_circle(renderer,
                           cx + fuse_length,
                           cy - 140, 7 + (int)(3 * sinf(time * 12)));

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
