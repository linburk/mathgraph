#include "../evaluate_rpn.h"
#include "SDL2/SDL.h"
#include "SDL2/SDL_ttf.h"
#include "interface.h"
#include "math.h"
#include "stdlib.h"

#define ERR(S, CRITICAL)                                                       \
  do {                                                                         \
    fprintf(stderr, "[%s:%d] %s error: { %s } \n", __FILE__, __LINE__, #S,     \
            SDL_GetError());                                                   \
    if (CRITICAL)                                                              \
      abort();                                                                 \
  } while (0)

struct gui_viewport {
  unsigned width;
  unsigned height;
  SDL_Window *window;
  SDL_Renderer *renderer;
};

#define ZOOM 1.25
#define SHIFT 30
#define WHEEL_SLOW 30
#define FPS 60
#define FONT_SIZE 30

#define BLACK 0x00, 0x00, 0x00, 0x00
#define WHITE 0xff, 0xff, 0xff, 0x00
#define RED 0xff, 0x00, 0x00, 0x00

const char font_path[] = "freesans.ttf";

static K1, K2, K3, C1, C2, C3;
#define RANDOM_COLOR(x)                                                        \
  (C1 + (x) * K1) % 0x100, (C2 + (x) * K2) % 0x100, (C3 + (x) * K3) % 0x100,   \
      0x00

int copy_text_to_renderer(SDL_Renderer *renderer, const char *text, int x,
                          int y) {
  if (SDL_SetRenderDrawColor(renderer, WHITE) != 0)
    ERR(SDL_SetRenderDrawColor, 0);
  SDL_Color black = {0, 0, 0, 0};
  TTF_Font *font = TTF_OpenFont("/Library/Fonts/Arial Unicode.ttf", FONT_SIZE);
  if (font == NULL)
    ERR(TTF_OpenFont, 1);
  SDL_Surface *text_surf = TTF_RenderText_Solid(font, text, black);
  if (text_surf == NULL)
    ERR(TTF_RenderText_Solid, 1);
  SDL_Texture *text_texture = SDL_CreateTextureFromSurface(renderer, text_surf);
  if (text_texture == NULL)
    ERR(SDL_CreateTextureFromSurface, 1);
  SDL_Rect text_box;
  text_box.x = x;
  text_box.y = y;
  text_box.w = text_surf->w;
  text_box.h = text_surf->h;
  if (SDL_RenderCopy(renderer, text_texture, NULL, &text_box) != 0)
    ERR(SDL_RenderCopy, 1);
  SDL_FreeSurface(text_surf);
  SDL_DestroyTexture(text_texture);
  TTF_CloseFont(font);
  return text_box.h;
}

void render_graph(SDL_Renderer *renderer, const struct graph_params graph) {
  {
    if (SDL_SetRenderDrawColor(renderer, WHITE) != 0)
      ERR(SDL_SetRenderDrawColor, 0);
    if (SDL_RenderClear(renderer) != 0)
      ERR(SDL_RenderClear, 0);
  }
  {
    if (SDL_SetRenderDrawColor(renderer, BLACK) != 0)
      ERR(SDL_SetRenderDrawColor, 0);
    SDL_Point axis_x[2], axis_y[2];
    for (int i = -1; i <= 1; ++i) { // 3pixel width
      axis_x[0].x = 0;
      axis_x[1].x = MAX_WIDTH;
      axis_x[0].y = axis_x[1].y =
          graph.height - 1 - (int)(-graph.base_y / graph.diff_y) + i;
      axis_y[0].y = 0;
      axis_y[1].y = MAX_HEIGHT;
      axis_y[0].x = axis_y[1].x = (int)(-graph.base_x / graph.diff_x) + i;
      if (SDL_RenderDrawLines(renderer, axis_x, 2) != 0)
        ERR(SDL_RenderDrawLines, 0);
      if (SDL_RenderDrawLines(renderer, axis_y, 2) != 0)
        ERR(SDL_RenderDrawLines, 0);
    }
  }
  {
    char x_text[25], y_text[25];
    memset(x_text, 0, 25);
    memset(y_text, 0, 25);
    int x, y;
    SDL_GetMouseState(&x, &y);
    sprintf(x_text, "x : %.3f", graph.base_x + (x * graph.diff_x));
    sprintf(y_text, "y : %.3f",
            graph.base_y + (graph.height - 1 - y) * graph.diff_y);
    int diff_h = copy_text_to_renderer(renderer, x_text, 10, 0);
    copy_text_to_renderer(renderer, y_text, 10, diff_h + 5);
  }
  {
    SDL_Point *points = (SDL_Point *)(calloc(graph.width, sizeof(SDL_Point)));
    if (points == NULL) {
      fprintf(stderr, "CALLOC ERROR\n");
      abort();
    }
    for (unsigned num = 0; num < graph.graph_count; ++num) {
      if (SDL_SetRenderDrawColor(renderer, RANDOM_COLOR(num)) != 0)
        ERR(SDL_SetRenderDrawColor, 0);
      unsigned ord = 0;
      for (int i = 0; i < graph.width; ++i, ++ord) {
        long double x = graph.base_x + i * graph.diff_x;
        long double y =
            evaluate(x, graph.rpn_expr[num], graph.rpn_expr_len[num]);
        if (isnan(y)) {
          if (ord > 1 && SDL_RenderDrawLines(renderer, points, ord) != 0)
            ERR(SDL_RenderDrawLines, 0);
          ord = -1;
          continue;
        }
        points[ord].x = i;
        points[ord].y =
            graph.height - 1 - (int)((y - graph.base_y) / graph.diff_y);
      }
      if (ord > 1 && SDL_RenderDrawLines(renderer, points, ord) != 0)
        ERR(SDL_RenderDrawLines, 0);
    }
    free(points);
  }
}

void render_frame(struct gui_viewport g, struct graph_params graph) {
  SDL_GetRendererOutputSize(g.renderer, &graph.width, &graph.height);
  Uint32 start = SDL_GetTicks();
  Uint32 elapsed;
  Uint32 estimated = 1000 / FPS;
  render_graph(g.renderer, graph);
  SDL_RenderPresent(g.renderer);
  elapsed = SDL_GetTicks() - start;
  if (elapsed < estimated)
    SDL_Delay(estimated - elapsed);
}

void gui_body(struct gui_viewport g, struct graph_params graph) {
  for (SDL_Event event;;) {
    render_frame(g, graph);
    if (SDL_PollEvent(&event) == 0)
      continue;
    switch (event.type) {
    case SDL_QUIT:
      return;
    case SDL_KEYDOWN:
      switch (event.key.keysym.sym) {
      case SDLK_ESCAPE:
        return;
      case SDLK_q:
        graph.diff_x *= ZOOM;
        graph.diff_y *= ZOOM;
        break;
      case SDLK_e:
        graph.diff_x /= ZOOM;
        graph.diff_y /= ZOOM;
        break;
      case SDLK_UP:
        graph.diff_y /= ZOOM;
        break;
      case SDLK_DOWN:
        graph.diff_y *= ZOOM;
        break;
      case SDLK_RIGHT:
        graph.diff_x /= ZOOM;
        break;
      case SDLK_LEFT:
        graph.diff_x *= ZOOM;
        break;
      case SDLK_w:
        graph.base_y += SHIFT * graph.diff_y;
        break;
      case SDLK_a:
        graph.base_x -= SHIFT * graph.diff_x;
        break;
      case SDLK_s:
        graph.base_y -= SHIFT * graph.diff_y;
        break;
      case SDLK_d:
        graph.base_x += SHIFT * graph.diff_x;
        break;
      default:
        break;
      }
      break;
    case SDL_MOUSEBUTTONDOWN: {
      int st_x, st_y, end_x, end_y;
      long double base_x = graph.base_x, base_y = graph.base_y;
      st_x = event.button.x, st_y = event.button.y;
      for (;;) {
        if (SDL_PollEvent(&event) == 0)
          continue;
        if (event.type == SDL_MOUSEMOTION) {
          render_frame(g, graph);
          end_x = event.motion.x, end_y = event.motion.y;
          graph.base_x = base_x - (end_x - st_x) * graph.diff_x;
          graph.base_y = base_y + (end_y - st_y) * graph.diff_y;
        }
        if (event.type == SDL_MOUSEBUTTONUP)
          break;
      }
    } break;
    case SDL_MOUSEWHEEL:
      if (event.wheel.preciseY > 0) {
        graph.diff_x /= (1 + event.wheel.preciseY / WHEEL_SLOW);
        graph.diff_y /= (1 + event.wheel.preciseY / WHEEL_SLOW);
      }
      if (event.wheel.preciseY < 0) {
        graph.diff_x *= (1 + -event.wheel.preciseY / WHEEL_SLOW);
        graph.diff_y *= (1 + -event.wheel.preciseY / WHEEL_SLOW);
      }
      break;
    default:
      break;
    }
  }
}

void gui(struct graph_params graph) {
  {
    srand(time(NULL));
    K1 = rand(), K2 = rand(), K3 = rand(), C1 = rand(), C2 = rand(),
    C3 = rand(); // random colors
  }
  struct gui_viewport g;
  Uint32 window_flags = SDL_WINDOW_RESIZABLE;
  Uint32 renderer_flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
  if (SDL_Init(SDL_INIT_VIDEO) != 0)
    ERR(SDL_Init(), 1);
  if (TTF_Init() != 0)
    ERR(TTF_Init, 1);
  atexit(SDL_Quit);
  g.window = SDL_CreateWindow("- Graff -", SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED, graph.width,
                              graph.height, window_flags);
  if (g.window == NULL)
    ERR(SDL_CreateWindow(), 1);
  g.renderer = SDL_CreateRenderer(g.window, -1, renderer_flags);
  if (g.renderer == NULL)
    ERR(SDL_CreateRenderer(), 1);
  gui_body(g, graph);
  SDL_DestroyRenderer(g.renderer);
  SDL_DestroyWindow(g.window);
}