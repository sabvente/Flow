#include <SDL.h>
#include <SDL_gfxPrimitives.h>
#include <SDL_framerate.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <stdio.h>

#define FOR_EACH(element, first) \
	for ((element) = (first); (element); (element) = (element)->next)

typedef enum GameState
{
	MainMenu,
	LevelSelectMenu,
	TimeTrialMenu,
	UserLevelLoading,
	AboutMenu,
	ActiveGame,
	GameOver,
	Exit
} GameState;

typedef enum FlowElementShape
{
	None = 0,
	UpS = (1<<0),
	RightS = (1<<1),
	DownS = (1<<2),
	LeftS = (1<<3),
	EndS = (1<<4)
} FlowElementShape;

typedef enum FlowDirection
{
	FromFirst = 1<<0,
	FromLast = 1<<1
}FlowDirection;

typedef struct FlowElement
{
	struct FlowElement *prev, *next;
	SDL_Rect position;
	FlowElementShape shape;
} FlowElement;

typedef struct Flow
{
	FlowElement *firstElement;
	FlowElement *lastElement;
	SDL_Color color;
	int completed;
	FlowDirection direction;
} Flow;

typedef enum LevelState
{
	Uncompleted, Completed, Starred
} LevelState;

typedef struct Level
{
	Flow *flows;
	LevelState state;
	unsigned int timeRecord;
	int size, flowCount;
} Level;

typedef	struct MenuItem
{
	char *name, mouseDown, mouseOver;
	GameState gameState;
	SDL_Color color;
} MenuItem;

typedef	struct TimeTrialMenuItem
{
	char *name, mouseDown, mouseOver;
	int index, time;
	SDL_Color color;
} TimeTrialMenuItem;

typedef	struct Button
{
	SDL_Rect position;
	GameState gameState;
	SDL_Surface *picture, *pictureDefault, *pictureMouseOver;
} Button;

typedef	struct LevelTile
{
	SDL_Rect position;
	char mouseDown;
} LevelTile;

typedef enum MouseButtonState
{
	Up, Down, JustUp, JustDown 
} MouseButtonState;

const int
	FONTSIZE_BIG = 130,
	FONTSIZE_NORMAL = 50,
	FONTSIZE_SMALL = 13,
	MAINMENU_ITEM_MARGIN = 200,
	LEVEL_TILE_MARGIN_TOP = 150,
	LEVEL_TILE_PADDING = 5,
	LEVEL_TILE_SIZE = 130,
	GAME_AREA_SIZE = 400,
	GAME_AREA_GRID_WIDTH = 3,
	FLOW_SIZE_PERCENT = 27,
	FLOW_END_SIZE_PERCENT = 54,
	FLOW_BG_OPACITY = 7,
	LEVEL_CHANGE_ARROW_DIST = 60,
	TIME_TRIAL_MARGIN_LEFT = 50;
const SDL_Color 
	LEVELTILE_COLOR = {96, 255, 47, 0},
	GAME_AREA_GRID_COLOR = {0, 15, 0, 0},
	FLOWCOLORS[] = {
		255, 0, 0, 0,
		255, 255, 0, 0, //yellow
		0, 255, 0, 0,
		0, 0, 255, 0,
		0, 255, 255, 0, //cyan
		255, 124, 0, 0, //orange
		140, 35, 163, 0,
};
SDL_TimerID userTimer;
SDL_Surface *screen, *icon, *starPic, *cMarkPic;
TTF_Font *fontTitle, *fontNormal, *fontSmall;
GameState gameState;
Level *currentLevels, *defaultLevels, *userLevels;
int currentLevelIndex, currentLevelCount, defaultLevelCount, userLevelCount,
	currentLevelSelectPage, levelSelectPageCount, innerFlowElementCount,
	completedFlowCount ,currentTimeTTime, currentTimeTScore,
	timeTHighScores[3], timeTScoreIndex;
char exiting, screenBlurred, loadUserLevel, KeysDown[SDLK_LAST + 1] = {0},
	*userLevelError = NULL, isTimeTrialGame;
Uint32 currentTime;
Uint32 aboutAnimation;
MouseButtonState LMB;
SDL_Rect mousePosition, mousePositionDown;
MenuItem mainMenuItems[] = {
	"arcade", 0, 0, LevelSelectMenu, {250,34,49},
	"time trial", 0, 0, TimeTrialMenu, {255,191,31},
	"user level", 0, 0, UserLevelLoading, {191,255,50},
	"about", 0, 0, AboutMenu, {171,227,25},
	"exit", 0, 0, Exit, {0, 196, 129}};
Button arrowBack = {12, 33, 0, 0, MainMenu, NULL, NULL, NULL}, 
	arrowNext = {0, 300, 0, 0, ActiveGame, NULL, NULL, NULL},
	arrowPrev = {0, 300, 0, 0, LevelSelectMenu, NULL, NULL, NULL},
	reload = {0, 300, 0, 0, ActiveGame, NULL, NULL, NULL},
	menuButton = {0, 300, 0, 0, LevelSelectMenu, NULL, NULL, NULL};
TimeTrialMenuItem timeTrialMenuItems[] = {
	"30 sec", 0, 0, 0, 30, {250,34,49},
	"60 sec", 0, 0, 1, 60, {255,191,31},
	"90 sec", 0, 0, 2, 90, {191,255,50}};
LevelTile levelTiles[9] = {0};
FlowElement *flowElementStart;
Flow *flowStart;

/// <summary>
/// Gets the maximum of two int.
/// </summary>
/// <param name="a">A number.</param>
/// <param name="b">A number.</param>
/// <returns>Returns the higher number</returns>
__inline int Max(int a, int b) {
	return a > b ? a : b;
}

/// <summary>
/// Draws a string on a screen at a position with specified foreground and background color.
/// </summary>
/// <param name="surface">The surface.</param>
/// <param name="rect">The target rect.</param>
/// <param name="font">The font.</param>
/// <param name="text">The string.</param>
/// <param name="fg">The foreground color.</param>
/// <param name="bg">The background color.</param>
void DrawString(SDL_Surface *surface, SDL_Rect rect, TTF_Font *font, char *text, SDL_Color fg, SDL_Color bg) 
{
	SDL_Surface *fontSurface = TTF_RenderText_Shaded(font, text, fg, bg);
	if(fontSurface)
	{
		SDL_BlitSurface(fontSurface, 0, screen, &rect);
		SDL_FreeSurface(fontSurface);
	}
}

/// <summary>
/// Darkens the specified color.
/// </summary>
/// <param name="color">The source color.</param>
/// <param name="amount">The amount of change (0-1).</param>
/// <returns>Returns the changed color.</returns>
SDL_Color Darken(SDL_Color color, double amount)
{
	SDL_Color ret;
	ret.r = (int) Max(0, color.r - 255 * amount);
	ret.g = (int) Max(0, color.g - 255 * amount);
	ret.b = (int) Max(0, color.b - 255 * amount);
	return ret;
}

/// <summary>
/// Converts SDL_Color to UInt32.
/// </summary>
/// <param name="color">The source color.</param>
/// <returns>Returns the converted color value.</returns>
Uint32 SDLColorTo32bit(SDL_Color color)
{
	Uint32 ret, t;
	t = color.r;
	ret = t << 24;
	t = color.g;
	ret |= t << 16;
	t = color.b;
	ret |= t << 8;
	return ret |= 0xff;
}

/// <summary>
/// Sets the opacity of a color.
/// </summary>
/// <param name="color">The source color.</param>
/// <param name="percent">The opacity percent.</param>
/// <returns>Returns the changed color.</returns>
Uint32 SetOpacity(Uint32 color, int percent)
{
	return ((color>>8)<<8) | (Uint32)((double)0xff * (double)percent / 100);
}

/// <summary>
/// Gets a pixel from the surface.
/// </summary>
/// <param name="surface">The surface.</param>
/// <param name="x">The x coordinate.</param>
/// <param name="y">The y coordinate.</param>
/// <returns>Returns the pixel.</returns>
Uint32 GetPixel(SDL_Surface *surface, int x, int y)
{
	Uint8 bpp = surface->format->BytesPerPixel;
	Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;
	switch (bpp)
	{
	case 1:
		return *p;
	case 2:
		return *(Uint16 *)p;
	case 3:
		if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
			return p[0] << 16 | p[1] << 8 | p[2];
		else
			return p[0] | p[1] << 8 | p[2] << 16;
	case 4:
		return *(Uint32 *)p;
	default:
		return 0;
	}
}

/// <summary>
/// Sets a pixel on the surface.
/// </summary>
/// <param name="surface">The surface.</param>
/// <param name="x">The x coordinate.</param>
/// <param name="y">The y coordinate.</param>
/// <param name="pixel">The pixel.</param>
void SetPixel(SDL_Surface *surface, int x, int y, Uint32 pixel)
{
	int bpp = surface->format->BytesPerPixel;
	Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;
	switch (bpp) 
	{
	case 1:
		*p = pixel;
		break;
	case 2:
		*(Uint16 *)p = pixel;
		break;
	case 3:
		if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
			p[0] = (pixel >> 16) & 0xff;
			p[1] = (pixel >> 8) & 0xff;
			p[2] = pixel & 0xff;
		}
		else {
			p[0] = pixel & 0xff;
			p[1] = (pixel >> 8) & 0xff;
			p[2] = (pixel >> 16) & 0xff;
		}
		break;
	case 4:
		*(Uint32 *)p = pixel;
		break;
	default:
		break;
	}
}

/// <summary>
/// Flips the surface horizontally.
/// </summary>
/// <param name="surface">The surface.</param>
/// <returns>Returns the flipped surface.</returns>
SDL_Surface *FlipH(SDL_Surface *surface)
{
	SDL_Surface *ret = NULL;
	int x, y, rx, ry;
	if(SDL_MUSTLOCK(surface))
		SDL_LockSurface(surface);
	ret = SDL_CreateRGBSurface(SDL_HWSURFACE, surface->w, surface->h,
		surface->format->BitsPerPixel, surface->format->Rmask,
		surface->format->Gmask, surface->format->Bmask, surface->format->Amask);
	for(x = 0, rx = ret->w - 1; x < ret->w; x++, rx--)
		for(y = 0, ry = ret->h - 1; y < ret->h; y++, ry--)
			SetPixel(ret, rx, y, GetPixel(surface, x, y));
	if(SDL_MUSTLOCK(surface))
		SDL_UnlockSurface(surface);
	return ret;
}

/// <summary>
/// Blurs the surface horizontally.
/// </summary>
/// <param name="surface">The surface.</param>
/// <param name="amount">The blur amount.</param>
void BlurH(SDL_Surface *surface, int amount)
{
	int x, y, hits, oldPixel, newPixel, bpp;
	Uint32 color, *newColors, r, g, b;
	bpp = surface->format->BytesPerPixel;
	newColors = (Uint32 *)malloc(sizeof(Uint32 *) * surface->w);
	for (y = 0; y < surface->h; y++)
	{
		hits = 0;
		r = g = b = 0;
		for (x = -amount; x < surface->w; x++)
		{
			oldPixel = x - amount - 1;
			if (oldPixel >= 0)
			{
				color = *(Uint32*)((Uint8 *)surface->pixels + y * surface->pitch + oldPixel * bpp);
				if (color != 0)
				{
					r -= (color & 0xff0000) >> 16;
					g -= (color & 0xff00) >> 8;
					b -= (color & 0xff);
				}
				hits--;
			}
			newPixel = x + amount;
			if (newPixel < surface->w)
			{
				color = *(Uint32*)((Uint8 *)surface->pixels + y * surface->pitch + newPixel * bpp);
				if (color != 0)
				{
					r += (color & 0xff0000) >> 16;
					g += (color & 0xff00) >> 8;
					b += (color & 0xff);
				}
				hits++;
			}
			if (x >= 0)
			{
				newColors[x] = (((int)((double)r / hits)) << 16)
					| (((int)((double)g / hits)) << 8)
					| (int)((double)b / hits);
			}
		}
		for (x = 0; x < surface->w; x++)
			*(Uint32*)((Uint8 *)surface->pixels + y * surface->pitch + x * bpp) = newColors[x];
	}
	free(newColors);
}

/// <summary>
/// Blurs the surface vertically.
/// </summary>
/// <param name="surface">The surface.</param>
/// <param name="amount">The blur amount.</param>
void BlurW(SDL_Surface *surface, int amount)
{
	int x, y, hits, oldPixel, newPixel, bpp;
	Uint32 color, *newColors, r, g, b;
	bpp = surface->format->BytesPerPixel;
	newColors = (Uint32 *)malloc(surface->h * sizeof(Uint32 *));
	for (x = 0; x < surface->w; x++)
	{
		hits = 0;
		r = g = b = 0;
		for (y = -amount; y < surface->h; y++)
		{
			oldPixel = y - amount - 1;
			if (oldPixel >= 0)
			{ 
				color = *(Uint32*)((Uint8 *)surface->pixels + oldPixel * surface->pitch + x * bpp);
				if (color != 0)
				{
					r -= (color & 0xff0000) >> 16;
					g -= (color & 0xff00) >> 8;
					b -= (color & 0xff);
				}
				hits--;
			}
			newPixel = y + amount;
			if (newPixel < surface->h)
			{
				color = *(Uint32*)((Uint8 *)surface->pixels + newPixel * surface->pitch + x * bpp);
				if (color != 0)
				{
					r += (color & 0xff0000) >> 16;
					g += (color & 0xff00) >> 8;
					b += (color & 0xff);
				}
				hits++;
			}
			if (y >= 0)
			{
				newColors[y] = (((int)((double)r / hits)) << 16)
					| (((int)((double)g / hits)) << 8)
					| (int)((double)b / hits);
			}
		}
		for (y = 0; y < surface->h; y++)
			*(Uint32*)((Uint8 *)surface->pixels + y * surface->pitch + x * bpp) = newColors[y];
	}
	free(newColors);
}


/// <summary>
/// Removes the flow element at the postition and removes the broken off FlowElements in a level.
/// </summary>
/// <param name="level">The level.</param>
/// <param name="xIn">The x coordinate.</param>
/// <param name="yIn">The y coordinate.</param>
/// <param name="removeThis">Determines whether to remove the FlowElement at the positon,
/// or remove only the rest.</param>
/// <returns>Returns 1 if FlowElement can be removed or the position is free,
/// 0 if the FlowElement at the position is the first or the last in a Flow.</returns>
int RemoveFlowElement(Level *level, int xIn, int yIn, char removeThis)
{
	Flow *f;
	FlowElement *fElem1, *fElem2;
	int a, b, i;
	for (i = 0; i < level->flowCount; i++)
	{
		f = &level->flows[i];
		FOR_EACH(fElem1, f->firstElement)
		{
			if (fElem1->position.x == xIn && fElem1->position.y == yIn)
			{
				if (f->completed)
				{
					if (fElem1 == f->firstElement || fElem1 == f->lastElement)
					{
						if (removeThis)
							return 0;
						else
						{
							RemoveFlowElement(level, f->firstElement->next->position.x, f->firstElement->next->position.y, 1);
							RemoveFlowElement(level, f->lastElement->prev->position.x, f->lastElement->prev->position.y, 1);
							f->direction = (FlowDirection)(FromFirst | FromLast);
							f->completed = 0;
							return 1;
						}
					}
					//remove from xy until end (remove the shorter part)
					for (fElem2 = fElem1->next, a = 0; fElem2 != f->lastElement; fElem2 = fElem2->next, a++);
					for (fElem2 = fElem1->prev, b = 0; fElem2 != f->firstElement; fElem2 = fElem2->prev, b++);
					if (a > b)
					{
						if (!removeThis) //skip one
							fElem1 = fElem1->prev;
						fElem1->next->prev = f->firstElement;
						f->firstElement->next = fElem1->next;
						f->direction = (FlowDirection)FromLast;
						f->completed = 0;
						RemoveFlowElement(level, f->lastElement->position.x, f->lastElement->position.y, 0);
					}
					else
					{
						if (!removeThis)
							fElem1 = fElem1->next;
						fElem1->prev->next = f->lastElement;
						f->lastElement->prev = fElem1->prev;
						f->direction = (FlowDirection)FromFirst;
						f->completed = 0;
						RemoveFlowElement(level, f->firstElement->position.x, f->firstElement->position.y, 1);
					}
					return 1;
				}
				else //not completed
				{
					if (removeThis)
					{
						if (fElem1 == f->firstElement || fElem1 == f->lastElement)
							return 0;
						if (f->direction & FromFirst)
						{
							fElem1->prev->next = f->lastElement;
							f->lastElement->prev = fElem1->prev;
						}
						else
						{
							fElem1->next->prev = f->firstElement;
							f->firstElement->next = fElem1->next;
						}
						do
						{
							fElem2 = (f->direction & FromFirst) ? fElem1->next : fElem1->prev;
							free(fElem1);
							fElem1 = fElem2;
						}
						while (!(fElem1 == f->lastElement || fElem1 == f->firstElement));
						return 1;
					}
					else
					{
						if (f->direction & FromFirst)
						{
							if (fElem1->next != NULL)
								return RemoveFlowElement(level, fElem1->next->position.x, fElem1->next->position.y, 1);
							else
								return 1;
						}
						else if (fElem1->prev != NULL)
							return RemoveFlowElement(level, fElem1->prev->position.x, fElem1->prev->position.y, 1);
						else
							return 1;
						return 0;
					}
				}
			}
		}
	}
	return 1;
}

/// <summary>
/// Frees the level.
/// </summary>
/// <param name="level">The level.</param>
void FreeLevelContent(Level *level)
{
	int i;
	if (level == NULL)
		return;
	for (i = level->flowCount - 1; i >= 0; i--)
	{
		RemoveFlowElement(level, level->flows[i].firstElement->next->position.x, level->flows[i].firstElement->next->position.y, 1);
		RemoveFlowElement(level, level->flows[i].lastElement->prev->position.x, level->flows[i].lastElement->prev->position.y, 1);
		free(level->flows[i].firstElement);
		free(level->flows[i].lastElement);
		level->flowCount--;
	}
	free(level->flows);
}

/// <summary>
/// Loads the levels from file.
/// </summary>
/// <param name="file">The file.</param>
/// <param name="levels">The levels.</param>
/// <param name="count">The level count.</param>
void LoadLevelsFromFile(FILE *file, Level **levels, int *count)
{
	char c;
	int i, flowColorIndex, countRet;
	Flow *flow, *flowsTemp;
	Level *levelRet = NULL, *levelTemp;
	*levels = NULL;
	*count = 0;

	countRet = 1;
	levelRet = (Level *)malloc(sizeof(Level));
	for(c = fgetc(file); c != EOF && c != '{'; c = fgetc(file));
	if (c == EOF)
		return;
	if((levelRet[countRet - 1].flows = (Flow *)malloc(sizeof(Flow))) == NULL)
		return;
	levelRet[countRet - 1].flowCount = 1;
	flowColorIndex = 0;
	while (1)
	{
		flow = &levelRet[countRet - 1].flows[levelRet[countRet - 1].flowCount - 1];
		if((flow->firstElement = (FlowElement *)malloc(sizeof(FlowElement))) == NULL)
			return;
		if((flow->lastElement = (FlowElement *)malloc(sizeof(FlowElement))) == NULL)
			return;
		flow->firstElement->prev = NULL;
		flow->firstElement->next = flow->lastElement;
		flow->firstElement->shape = EndS;
		flow->lastElement->next = NULL;
		flow->lastElement->prev = flow->firstElement;
		flow->lastElement->shape = EndS;
		if(fscanf(file, "{%d,%d,%d,%d},",
			&(flow->firstElement->position.x),
			&(flow->firstElement->position.y),
			&(flow->lastElement->position.x),
			&(flow->lastElement->position.y)) < 4)
			return;
		flow->firstElement->position.x--;
		flow->firstElement->position.y--;
		flow->lastElement->position.x--;
		flow->lastElement->position.y--;
		flow->color = FLOWCOLORS[flowColorIndex++];
		flow->completed = 0;
		flow->direction = (FlowDirection)(FromFirst | FromLast);

		if(fscanf(file, "%d,", &(levelRet[countRet - 1].size)) < 1) //there are more flows to read
		{
			if((flowsTemp = (Flow *)malloc(sizeof(Flow) * (levelRet[countRet - 1].flowCount + 1))) == NULL)
				return;
			for (i = 0; i < levelRet[countRet - 1].flowCount; i++)
				flowsTemp[i] = levelRet[countRet - 1].flows[i];
			free(levelRet[countRet - 1].flows);
			levelRet[countRet - 1].flows = flowsTemp;
			levelRet[countRet - 1].flowCount++;
		}
		else
		{
			if(fscanf(file, "%d,%d}",
				&(levelRet[countRet - 1].state),
				&(levelRet[countRet - 1].timeRecord)) < 2)
				return;
			for(c = fgetc(file); c != EOF && c != '{'; c = fgetc(file));
			if (c == EOF)
			{
				*count = countRet;
				*levels = levelRet;
				return;
			}
			levelTemp = (Level *)malloc(sizeof(Level) * (countRet + 1));
			for (i = 0; i < countRet; i++)
				levelTemp[i] = levelRet[i];
			free(levelRet);
			levelRet = levelTemp;
			countRet++;
			if((levelRet[countRet - 1].flows = (Flow *)malloc(sizeof(Flow))) == NULL)
				return;
			levelRet[countRet - 1].flowCount = 1;
			flowColorIndex = 0;
		}
	}
}

/// <summary>
/// Makes route from flowElemetStart to a position if it is possible
/// </summary>
/// <param name="x">The x coordinate.</param>
/// <param name="y">The y coordinate.</param>
/// <returns>Returns -1 on error, 0 otherwise.</returns>
int MakeRoute(int x, int y)
{
	int i, j, k, l;
	FlowElement *fe1;
	if (flowElementStart == NULL || flowStart == NULL)
		return -1;
	if (flowStart->completed)
	{
		if (flowElementStart->position.x == x && flowElementStart->position.y == y)
			RemoveFlowElement(&currentLevels[currentLevelIndex], flowElementStart->position.x, flowElementStart->position.y, 0);
		else //completed flow, but not released mouse yet
		{
			//find flowelement if it is under xy
			FOR_EACH(fe1, flowStart->firstElement)
			{
				if (fe1->position.x == x && fe1->position.y == y)
				{
					if (fe1 == flowStart->firstElement->next &&
						(fe1 != flowStart->lastElement || flowElementStart == flowStart->lastElement))
					{
						flowStart->direction = (FlowDirection)FromLast;
						flowStart->completed = 0;
					}
					else if(fe1 == flowStart->lastElement->prev &&
						(fe1 != flowStart->firstElement || flowElementStart == flowStart->firstElement))
					{
						flowStart->direction = (FlowDirection)FromFirst;
						flowStart->completed = 0;
					}
					break;
				}
			}
		}
	}
	else //not completed
	{
		if (flowElementStart == flowStart->firstElement)
			flowStart->direction = (FlowDirection)FromFirst;
		else if (flowElementStart == flowStart->lastElement)
			flowStart->direction = (FlowDirection)FromLast;
		//starts from the first
		if (flowStart->direction & FromFirst)
		{
			//remove flowElements after xy
			FOR_EACH(fe1, flowStart->firstElement)
			{
				if (fe1->position.x == x && fe1->position.y == y) //xy is in flowStart
					RemoveFlowElement(&currentLevels[currentLevelIndex], fe1->position.x, fe1->position.y, 0);
			}
			//add new FLowElements (xy is not in FLowStart)
			fe1 = flowStart->lastElement->prev;
			k = y - fe1->position.y;
			l = x - fe1->position.x;
			//advenced route not yet implemented, only left and right connection
			if (!(((k == 0) && (l != 0)) || ((l == 0) && (k != 0))))
				return 0;
			i = fe1->position.y + (k == 0 ? 0 : (k > 0 ? 1 : -1));
			do
			{
				j = fe1->position.x + (l == 0 ? 0 : (l > 0 ? 1 : -1));
				do
				{
					//flow completed
					if (flowStart->lastElement->position.x == j && flowStart->lastElement->position.y == i)
					{
						fe1->next = flowStart->lastElement;
						flowStart->lastElement->prev = fe1;
						flowStart->completed = 1;
						flowStart->direction = (FlowDirection)(FromFirst | FromLast);
						return 0;
					}
					if(RemoveFlowElement(&currentLevels[currentLevelIndex], j, i, 1))
					{
						if((fe1->next = (FlowElement *)malloc(sizeof(FlowElement))) == NULL)
							return -1;
						flowStart->lastElement->prev = fe1->next;
						fe1->next->next = flowStart->lastElement;
						fe1->next->prev = fe1;
						fe1->next->position.x = j;
						fe1->next->position.y = i;
						fe1 = flowStart->lastElement->prev;
						k = y - fe1->position.y;
						l = x - fe1->position.x;
						if (!(((k == 0) && (l != 0)) || ((l == 0) && (k != 0))))
							return 0;
					}
					else
						return 0;
					j += (l == 0 ? 0 : (l > 0 ? 1 : -1));
				}while (j != x + l);
				i += (k == 0 ? 0 : (k > 0 ? 1 : -1));
			}while(i != y + k);
		}
		//starts from the last
		else
		{
			//remove flowElements after xy
			FOR_EACH(fe1, flowStart->firstElement)
			{
				if (fe1->position.x == x && fe1->position.y == y)//xy is in FlowStart
					RemoveFlowElement(&currentLevels[currentLevelIndex], fe1->position.x, fe1->position.y, 0);
			}
			//add new FLowElements (xy is not in FLowStart)
			fe1 = flowStart->firstElement->next;
			l = x - fe1->position.x;
			k = y - fe1->position.y;
			if (!(((k == 0) && (l != 0)) || ((l == 0) && (k != 0))))
				return 0;
			i = fe1->position.y + (k == 0 ? 0 : (k > 0 ? 1 : -1));
			do
			{
				j = fe1->position.x + (l == 0 ? 0 : (l > 0 ? 1 : -1));
				do
				{
					//flow completed
					if (flowStart->firstElement->position.x == j && flowStart->firstElement->position.y == i)
					{
						fe1->prev = flowStart->firstElement;
						flowStart->firstElement->next = fe1;
						flowStart->completed = 1;
						flowStart->direction = (FlowDirection)(FromFirst | FromLast);
						return 0;
					}
					if(RemoveFlowElement(&currentLevels[currentLevelIndex], j, i, 1))
					{
						fe1->prev = (FlowElement *)malloc(sizeof(FlowElement));
						if (fe1->prev == NULL)
							return -1;
						flowStart->firstElement->next = fe1->prev;
						fe1->prev->prev = flowStart->firstElement;
						fe1->prev->next = fe1;
						fe1->prev->position.x = j;
						fe1->prev->position.y = i;
						fe1 = flowStart->firstElement->next;
						l = x - fe1->position.x;
						k = y - fe1->position.y;
						if (!(((k == 0) && (l != 0)) || ((l == 0) && (k != 0))))
							return 0;
					}
					else
						return 0;
					j += l == 0 ? 0 : (l > 0 ? 1 : -1);
				}while (j != x + l);
				i += k == 0 ? 0 : (k > 0 ? 1 : -1);
			}while(i != y + k);
		}
	}
	return 0;
}

/// <summary>
/// Determines whether the button is clicked.
/// </summary>
/// <param name="button">The button.</param>
/// <returns>Returns 1 if clicked, otherwise 0</returns>
int IsButtonClicked(Button *button)
{
	if (mousePosition.x >= button->position.x && mousePosition.x < button->position.x + button->picture->w &&
		mousePosition.y >= button->position.y && mousePosition.y < button->position.y + button->picture->h)
		if(LMB == JustUp)
			return 1;
		else
			button->picture = button->pictureMouseOver;
	else
		button->picture = button->pictureDefault;
	return 0;
}

/// <summary>
/// Determines whether a point is in the rectengle.
/// </summary>
/// <param name="x">The x coordinate.</param>
/// <param name="y">The y coordinate.</param>
/// <param name="w">The width.</param>
/// <param name="h">The height.</param>
/// <param name="point">The point.</param>
/// <returns>Returns 1 if the point is in the rectangle, 0 otherwise.</returns>
int InRect(int x, int y, int w, int h, SDL_Rect point)
{
	return point.x >= x && point.x < x + w && point.y >= y && point.y < y + h;
}

/// <summary>
/// Adds connection to a flowElement by setting it's shape
/// </summary>
/// <param name="from">The FlowElement to add connection from.</param>
/// <param name="to">The FlowElement to add connection.</param>
void ShapeAddConnection(FlowElement *from, FlowElement *to)
{
	if (from->position.x > to->position.x)
	{
		to->shape = (FlowElementShape)(to->shape | RightS);
	}
	else if(from->position.x < to->position.x)
	{
		to->shape = (FlowElementShape)(to->shape | LeftS);
	}
	else if (from->position.y > to->position.y)
	{
		to->shape = (FlowElementShape)(to->shape | DownS);
	}
	else
	{
		to->shape = (FlowElementShape)(to->shape | UpS);
	}
}

/// <summary>
/// Updates the FlowElement shapes
/// </summary>
void UpdateShapes()
{
	Level *level;
	FlowElement *feA;
	int i;
	level = &currentLevels[currentLevelIndex];
	for (i = 0; i < level->flowCount; i++)
	{
		FOR_EACH(feA, level->flows[i].firstElement)
		{
			feA->shape = (feA == level->flows[i].firstElement || feA == level->flows[i].lastElement) ? EndS : None;
			if (level->flows[i].completed)
			{
				if (feA->next != NULL)
				{
					ShapeAddConnection(feA->next, feA);
				}
				if (feA->prev != NULL)
				{
					ShapeAddConnection(feA->prev, feA);
				}
			}
			else
			{
				if (feA->next != NULL &&
					((feA->next != level->flows[i].lastElement) || (level->flows[i].direction & FromLast) && (feA != level->flows[i].firstElement)) &&
					((feA != level->flows[i].firstElement) || (level->flows[i].direction & FromFirst) && (feA->next != level->flows[i].lastElement)))
				{
					ShapeAddConnection(feA->next, feA);
				}
				if (feA->prev != NULL &&
					((feA->prev != level->flows[i].firstElement) || (level->flows[i].direction & FromFirst) && (feA != level->flows[i].lastElement)) &&
					((feA != level->flows[i].lastElement) || (level->flows[i].direction & FromLast) && (feA->prev != level->flows[i].firstElement)))
				{
					ShapeAddConnection(feA->prev, feA);
				}
			}
		}
	}
}

/// <summary>
/// Sets the current level.
/// </summary>
/// <param name="levelIndex">Index of the currentLevels.</param>
void SetCurrentLevel(int levelIndex)
{
	int i;
	if (currentLevelCount > 1)
	{
		if (levelIndex > currentLevelCount - 1)
			levelIndex = currentLevelCount - 1;
		else if (levelIndex < 0)
			levelIndex = 0;
	}
	else
		return;
	currentLevelIndex = levelIndex;
	innerFlowElementCount = 0;
	completedFlowCount = 0;
	//reset currentLevels
	for (i = 0; i < currentLevels[levelIndex].flowCount; i++)
	{
		RemoveFlowElement(&currentLevels[levelIndex], currentLevels[levelIndex].flows[i].firstElement->position.x,
			currentLevels[levelIndex].flows[i].firstElement->position.y, 0);
		RemoveFlowElement(&currentLevels[levelIndex], currentLevels[levelIndex].flows[i].lastElement->position.x,
			currentLevels[levelIndex].flows[i].lastElement->position.y, 0);
	}
	UpdateShapes();
}

/// <summary>
/// Processes the SDL events.
/// </summary>
void ProcessEvents()
{
	SDL_Event event;
	if(SDL_WaitEvent(&event))
		switch (event.type)
	{
		case SDL_QUIT:
			exiting = 1;
			break;
		case SDL_KEYDOWN:
			KeysDown[event.key.keysym.sym] = 1;
			break;
		case SDL_KEYUP:
			KeysDown[event.key.keysym.sym] = 0;
			break;
		case SDL_MOUSEMOTION:
			mousePosition.x = event.motion.x;
			mousePosition.y = event.motion.y;
			break;
		case SDL_MOUSEBUTTONDOWN:
			if (event.button.button == 1)
			{
				mousePositionDown.x = mousePosition.x;
				mousePositionDown.y = mousePosition.y;
				LMB = JustDown;
			}
			break;
		case SDL_MOUSEBUTTONUP:
			if (event.button.button == 1)
				LMB = JustUp;
			break;
		default:
			break;
	}
}

void Draw();

/// <summary>
/// Updates the game logic.
/// </summary>
/// <returns>Returns -1 on error, 0 otherwise.</returns>
int Update()
{
	int i, j, textW, textH, margin;
	SDL_Rect v;
	FlowElement *fElem1;
	switch (gameState)
	{
	case MainMenu:
		v.y = MAINMENU_ITEM_MARGIN;
		for (i = 0; i < sizeof(mainMenuItems)/sizeof(MenuItem); i++)
		{
			TTF_SizeText(fontNormal, mainMenuItems[i].name, &textW, &textH);
			v.x = (screen->w - textW) / 2;
			if (InRect(v.x, v.y, textW, textH, mousePosition))
			{
				mainMenuItems[i].mouseOver = 1;
				if(LMB == JustDown)
					mainMenuItems[i].mouseDown = 1;
				else if(LMB == JustUp && mainMenuItems[i].mouseDown)
				{
					gameState = mainMenuItems[i].gameState;
					mainMenuItems[i].mouseDown = 0;
					Update();
					break;
				}
			}
			else
			{
				mainMenuItems[i].mouseOver = 0;
				mainMenuItems[i].mouseDown = 0;
			}
			v.y += textH;
		}
		break;

	case LevelSelectMenu:
		currentLevels = defaultLevels;
		currentLevelCount = defaultLevelCount;
		arrowBack.gameState = MainMenu;
		arrowNext.position.y = LEVEL_TILE_MARGIN_TOP + 3 * LEVEL_TILE_SIZE + 2 * LEVEL_TILE_PADDING + 20;
		arrowPrev.position.y = arrowNext.position.y;
		menuButton.gameState = LevelSelectMenu;
		//Back button
		if (IsButtonClicked(&arrowBack))
			gameState = arrowBack.gameState;
		if (currentLevelSelectPage > 0 && IsButtonClicked(&arrowPrev))
			currentLevelSelectPage--;
		if (currentLevelSelectPage < levelSelectPageCount - 1 && IsButtonClicked(&arrowNext))
			currentLevelSelectPage++;
		//Tiles
		margin = (screen->w-(3 * LEVEL_TILE_SIZE + LEVEL_TILE_PADDING * 2)) / 2;
		v.y = LEVEL_TILE_MARGIN_TOP;
		for (i = 0; i < 3; i++)
		{
			v.x = margin;
			for (j = 0; j < 3; j++)
			{
				if (InRect(v.x, v.y, LEVEL_TILE_SIZE, LEVEL_TILE_SIZE, mousePosition))
				{
					if(LMB == JustDown)
						levelTiles[i * 3 + j].mouseDown = 1;
					else
					{
						if(LMB == JustUp && levelTiles[i * 3 + j].mouseDown)
						{
							SetCurrentLevel(i * 3 + j + currentLevelSelectPage * 9);
							gameState = ActiveGame;
							Update();
							arrowBack.gameState = LevelSelectMenu;
							levelTiles[i * 3 + j].mouseDown = 0;
						}
					}
				}
				else
					levelTiles[i * 3 + j].mouseDown = 0;
				v.x += LEVEL_TILE_SIZE + LEVEL_TILE_PADDING;
			}
			v.y += LEVEL_TILE_SIZE + LEVEL_TILE_PADDING;
		}
		break;

	case ActiveGame:
		arrowNext.position.y = LEVEL_TILE_MARGIN_TOP + 3 * LEVEL_TILE_SIZE + 2 * LEVEL_TILE_PADDING + 20;
		arrowPrev.position.y = arrowNext.position.y;
		reload.position.y = arrowNext.position.y;
		reload.position.x = screen->w / 2 - reload.picture->w / 2;
		if (IsButtonClicked(&arrowBack))
			gameState = arrowBack.gameState;
		if (IsButtonClicked(&reload))
			SetCurrentLevel(currentLevelIndex);
		if (!isTimeTrialGame)
		{
			//next currentLevels button
			if (currentLevelIndex != currentLevelCount - 1 && IsButtonClicked(&arrowNext))
			{
				SetCurrentLevel(currentLevelIndex + 1);
				if (currentLevels == defaultLevels)
					currentLevelSelectPage = (currentLevelIndex) / 9;
			}
			else if (currentLevelIndex > 0 && IsButtonClicked(&arrowPrev))
			{
				SetCurrentLevel(currentLevelIndex - 1);
				if (currentLevels == defaultLevels)
					currentLevelSelectPage = (currentLevelIndex) / 9;
			}
		}
		else if (SDL_GetTicks() - 1000 > currentTime)
		{
			currentTime = SDL_GetTicks();
			if (--currentTimeTTime == -1)
			{
				gameState = GameOver;
				menuButton.gameState = TimeTrialMenu;
				Update();
			}
		}
		margin = (screen->w - GAME_AREA_SIZE) / 2;
		if (InRect(margin, LEVEL_TILE_MARGIN_TOP, GAME_AREA_SIZE, GAME_AREA_SIZE, mousePosition))
		{
			//get clicked flow element
			if(LMB == JustDown)
			{
				flowElementStart = NULL;
				flowStart = NULL;
				v.w = GAME_AREA_SIZE / currentLevels[currentLevelIndex].size; //flow element size
				for (i = 0; i < currentLevels[currentLevelIndex].flowCount; i++)
				{
					FOR_EACH(fElem1, currentLevels[currentLevelIndex].flows[i].firstElement)
					{
						if(InRect(fElem1->position.x * v.w + margin,
							fElem1->position.y * v.w + LEVEL_TILE_MARGIN_TOP, v.w, v.w, mousePosition))
						{
							flowStart = &currentLevels[currentLevelIndex].flows[i];
							flowElementStart = fElem1;
							if (MakeRoute(flowElementStart->position.x, flowElementStart->position.y) == -1)
								return -1; //memory error
							UpdateShapes();
							break;
						}
					}
				}
			}
			else if (LMB == Down && flowElementStart != NULL && flowStart != NULL)
			{
				//convert mouse position to FlowElement posisiton
				v.x = (double)(mousePosition.x - margin) / ((double)GAME_AREA_SIZE / currentLevels[currentLevelIndex].size);
				v.y = (double)(mousePosition.y - LEVEL_TILE_MARGIN_TOP - 1) / ((double)GAME_AREA_SIZE / currentLevels[currentLevelIndex].size);
				//connect FlowElements
				if (MakeRoute(v.x, v.y) == -1)
					return -1; //memory error
				UpdateShapes();
				//count completed Flows
				completedFlowCount = 0;
				innerFlowElementCount = 0;
				for (i = 0; i < currentLevels[currentLevelIndex].flowCount; i++)
				{
					if (currentLevels[currentLevelIndex].flows[i].completed)
						completedFlowCount++;
					FOR_EACH(fElem1, currentLevels[currentLevelIndex].flows[i].firstElement)
						innerFlowElementCount++;
					innerFlowElementCount -= 2; //substract first and last
				}
				//check if game is over
				if (currentLevels[currentLevelIndex].flowCount == completedFlowCount)
				{
					if (innerFlowElementCount + currentLevels[currentLevelIndex].flowCount * 2 ==
						currentLevels[currentLevelIndex].size * currentLevels[currentLevelIndex].size)
						currentLevels[currentLevelIndex].state = Starred;
					else if (currentLevels[currentLevelIndex].state != Starred)
						currentLevels[currentLevelIndex].state = Completed;
					Draw(); //draw the last connection
					if (isTimeTrialGame)
					{
						currentTimeTScore++;
						SetCurrentLevel(rand() % defaultLevelCount);
					}
					else
					{
						gameState = GameOver;
						Update();
						screenBlurred = 0;
					}
					LMB = Up;
				}
			}
		}
		break;

	case GameOver:
		reload.position.x = screen->w / 2 - (int)(reload.picture->w * 0.5) - LEVEL_CHANGE_ARROW_DIST;
		reload.position.y = 300;
		arrowNext.position.y = 300;
		arrowNext.gameState = ActiveGame;
		if (IsButtonClicked(&menuButton))
			gameState = menuButton.gameState;
		if (!isTimeTrialGame)
		{
			//reload level button
			if (currentLevels[currentLevelIndex].state != Starred && IsButtonClicked(&reload))
			{
				gameState = reload.gameState;
				reload.picture = reload.pictureDefault;
				SetCurrentLevel(currentLevelIndex);
			}
			//next level button
			if (currentLevelIndex != currentLevelCount - 1 && IsButtonClicked(&arrowNext))
			{
				SetCurrentLevel(currentLevelIndex + 1);
				currentLevelSelectPage = (currentLevelIndex + 1) / 10;
				gameState = arrowNext.gameState;
				Update();
			}
		}
		else if (timeTHighScores[timeTScoreIndex] < currentTimeTScore)
			timeTHighScores[timeTScoreIndex] = currentTimeTScore;
		break;

	case TimeTrialMenu:
		currentLevels = defaultLevels;
		arrowBack.gameState = MainMenu;
		isTimeTrialGame = 0;
		if (IsButtonClicked(&arrowBack))
			gameState = arrowBack.gameState;
		v.y = MAINMENU_ITEM_MARGIN;
		v.x = TIME_TRIAL_MARGIN_LEFT;
		for (i = 0; i < sizeof(timeTrialMenuItems)/sizeof(TimeTrialMenuItem); i++)
		{
			TTF_SizeText(fontNormal, timeTrialMenuItems[i].name, &textW, &textH);
			if (InRect(v.x, v.y, textW, textH, mousePosition))
			{
				timeTrialMenuItems[i].mouseOver = 1;
				if(LMB == JustDown)
					timeTrialMenuItems[i].mouseDown = 1;
				else if(LMB == JustUp && timeTrialMenuItems[i].mouseDown)
				{
					gameState = ActiveGame;
					timeTrialMenuItems[i].mouseDown = 0;
					timeTrialMenuItems[i].mouseOver = 0;
					currentTimeTTime = timeTrialMenuItems[i].time + 1;
					timeTScoreIndex = timeTrialMenuItems[i].index;
					isTimeTrialGame = 1;
					currentTimeTScore = 0;
					SetCurrentLevel(rand() % defaultLevelCount);
					arrowBack.gameState = TimeTrialMenu;
					Update();
				}
			}
			else
			{
				timeTrialMenuItems[i].mouseDown = 0;
				timeTrialMenuItems[i].mouseOver = 0;
			}
			v.y += textH;
		}
		break;

	case UserLevelLoading:
		if (loadUserLevel)
		{
			FILE *file;
			loadUserLevel = 0;
			menuButton.gameState = MainMenu;
			if ((file = fopen("userLevels.txt", "rt")) == NULL)
			{
				userLevelError = "\"userLevels.txt\" could not be loaded.";
				userLevels = NULL;
				userLevelCount = 0;
			}
			else
			{
				if (userLevels) //free previously loaded currentLevels
				{
					for (i = 0; i < userLevelCount; i++)
						FreeLevelContent(&userLevels[i]);
					free(userLevels);
				}
				LoadLevelsFromFile(file, &userLevels, &userLevelCount);
				if (!userLevels)
					userLevelError = "\"userLevels.txt\" contains wrong format.";
				else
				{
					currentLevels = userLevels;
					currentLevelCount = userLevelCount;
					SetCurrentLevel(0);
					loadUserLevel = 1;
					gameState = ActiveGame;
					Update();
				}
				fclose(file);
			}
			screenBlurred = 0;
		}
		if (IsButtonClicked(&menuButton))
		{
			gameState = menuButton.gameState;
			loadUserLevel = 1;
		}
		break;

	case AboutMenu:
		if (IsButtonClicked(&arrowBack))
			gameState = arrowBack.gameState;
		break;

	case Exit:
		exiting = 1;
		break;
	}
	switch (LMB)
	{
	case JustUp:
		LMB = Up;
		break;
	case JustDown:
		LMB = Down;
		break;
	default:
		break;
	}
	if (KeysDown[SDLK_ESCAPE])
		exiting = 1;
	return 0;
}

/// <summary>
/// Draws the game.
/// </summary>
void Draw()
{
	SDL_Color white = {255,255,255};
	SDL_Color black = {0,0,0};
	char str[60];
	int i, j, k, textW, textH, margin;
	SDL_Rect r = {0, 0, 0, 0};
	Flow *f;
	FlowElement *fElem1;
	switch (gameState)
	{
	case MainMenu:
		SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));
		TTF_SizeText(fontTitle,"flow", &textW, NULL);
		r.x = (screen->w - textW) / 2;
		DrawString(screen, r, fontTitle, "flow", white, black);
		r.y = MAINMENU_ITEM_MARGIN;
		for (i = 0; i < sizeof(mainMenuItems)/sizeof(MenuItem); i++)
		{
			TTF_SizeText(fontNormal, mainMenuItems[i].name, &textW, &textH);
			r.x = (screen->w - textW) / 2;
			if (mainMenuItems[i].mouseOver)
			{
				if (mainMenuItems[i].mouseDown)
					DrawString(screen, r, fontNormal, mainMenuItems[i].name, Darken(mainMenuItems[i].color, 0.3), mainMenuItems[i].color);
				else
					DrawString(screen, r, fontNormal, mainMenuItems[i].name, Darken(mainMenuItems[i].color, 0.3), black);
			}
			else
				DrawString(screen, r, fontNormal, mainMenuItems[i].name, mainMenuItems[i].color, black);
			r.y += textH;
		}
		break;

	case LevelSelectMenu:
		SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));
		r.x = arrowBack.position.x;
		r.y = arrowBack.position.y;
		SDL_BlitSurface(arrowBack.picture, 0, screen, &r);
		if (currentLevelSelectPage > 0)
		{
			r.x = arrowPrev.position.x;
			r.y = arrowPrev.position.y;
			SDL_BlitSurface(arrowPrev.picture, 0, screen, &r);
		}
		if (currentLevelSelectPage < levelSelectPageCount - 1)
		{
			r.x = arrowNext.position.x;
			r.y = arrowNext.position.y;
			SDL_BlitSurface(arrowNext.picture, 0, screen, &r);
		}

		//current/all LevelTile page
		*str = 0;
		sprintf(str, "%d/%d", currentLevelSelectPage + 1, levelSelectPageCount);
		TTF_SizeText(fontSmall, str, &textW, NULL);
		r.x = screen->w / 2 - textW / 2;
		r.y = LEVEL_TILE_MARGIN_TOP - 30;
		DrawString(screen, r, fontSmall, str, white, black);

		//Tiles
		margin = (screen->w - (3 * LEVEL_TILE_SIZE + LEVEL_TILE_PADDING * 2)) / 2;
		r.y = LEVEL_TILE_MARGIN_TOP;
		for (i = 0; i < 3; i++)
		{
			r.x = margin;
			for (j = 0; j < 3; j++)
			{
				sprintf(str, "%d", i * 3 + j + 1 + currentLevelSelectPage * 9);
				TTF_SizeText(fontNormal, str, &textW, &textH);
				if (levelTiles[i * 3 + j].mouseDown)
				{
					boxColor(screen, r.x, r.y, r.x + LEVEL_TILE_SIZE, r.y + LEVEL_TILE_SIZE, 
						SDLColorTo32bit(Darken(LEVELTILE_COLOR, 0.3)));
					r.x += (LEVEL_TILE_SIZE - textW) / 2;
					r.y += (LEVEL_TILE_SIZE - textH) / 2;
					DrawString(screen, r, fontNormal, str, black, Darken(LEVELTILE_COLOR, 0.3));
					r.x -= (LEVEL_TILE_SIZE - textW) / 2;
					r.y -= (LEVEL_TILE_SIZE - textH) / 2;
				}
				else
				{
					boxColor(screen, r.x, r.y, r.x + LEVEL_TILE_SIZE, r.y + LEVEL_TILE_SIZE, 
						SDLColorTo32bit(LEVELTILE_COLOR));
					r.x += (LEVEL_TILE_SIZE - textW) / 2;
					r.y += (LEVEL_TILE_SIZE - textH) / 2;
					DrawString(screen, r, fontNormal, str, black, LEVELTILE_COLOR);
					r.x -= (LEVEL_TILE_SIZE - textW) / 2;
					r.y -= (LEVEL_TILE_SIZE - textH) / 2;
				}
				//level complete indicator
				if (i * 3 + j + currentLevelSelectPage * 9 < currentLevelCount)
				{
					r.x += LEVEL_TILE_SIZE - cMarkPic->w - 3;
					r.y += LEVEL_TILE_SIZE - cMarkPic->h - 3;
					switch (currentLevels[i * 3 + j + currentLevelSelectPage * 9].state)
					{
					case Completed:
						SDL_BlitSurface(cMarkPic, 0, screen, &r);
						break;
					case Starred:
						SDL_BlitSurface(starPic, 0, screen, &r);
						break;
					default:
						break;
					}
					r.y -= LEVEL_TILE_SIZE - cMarkPic->h - 3;
					r.x -= LEVEL_TILE_SIZE - cMarkPic->w - 3;
				}
				r.x += LEVEL_TILE_SIZE + LEVEL_TILE_PADDING;
			}
			r.y += LEVEL_TILE_SIZE + LEVEL_TILE_PADDING;
		}
		r.x = 50;
		r.y = 0;
		DrawString(screen, r, fontNormal, "choose levels", white, black);
		break;

	case ActiveGame:
		SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));
		//back button
		r.x = arrowBack.position.x;
		r.y = arrowBack.position.y;
		SDL_BlitSurface(arrowBack.picture, 0, screen, &r);
		if (!isTimeTrialGame)
		{
			//previous currentLevels button
			if (currentLevelIndex > 0)
			{
				r.x = arrowPrev.position.x;
				r.y = arrowPrev.position.y;
				SDL_BlitSurface(arrowPrev.picture, 0, screen, &r);
			}
			//next currentLevels button
			if (currentLevelIndex != currentLevelCount - 1)
			{
				r.x = arrowNext.position.x;
				r.y = arrowNext.position.y;
				SDL_BlitSurface(arrowNext.picture, 0, screen, &r);
			}
		}
		else
		{
			//time left
			*str = 0;
			sprintf(str, "%d", currentTimeTTime);
			TTF_SizeText(fontSmall, str, &textW, &textH);
			r.x = (screen->w + GAME_AREA_SIZE) / 2 - 20;
			r.y = LEVEL_TILE_MARGIN_TOP - textH - 10;
			DrawString(screen, r, fontSmall, str, white, black);
			//current score
			*str = 0;
			sprintf(str, "completed: %d", currentTimeTScore);
			TTF_SizeText(fontSmall, str, &textW, &textH);
			r.x = (screen->w + textW) / 2;
			r.y = LEVEL_TILE_MARGIN_TOP - textH - 10;
			DrawString(screen, r, fontSmall, str, white, black);
		}
		//reload currentLevels button
		r.x = reload.position.x;
		r.y = reload.position.y;
		SDL_BlitSurface(reload.picture, 0, screen, &r);
		margin = (screen->w - GAME_AREA_SIZE)/2;
		//comleted/all flow count
		*str = 0;
		sprintf(str, "flows: %d/%d", completedFlowCount, currentLevels[currentLevelIndex].flowCount);
		TTF_SizeText(fontSmall, str, &textW, &textH);
		r.x = margin;
		r.y = LEVEL_TILE_MARGIN_TOP - textH - 10;
		DrawString(screen, r, fontSmall, str, white, black);
		//percent
		*str = 0;
		sprintf(str, "%d%%",
			(int)((double)(innerFlowElementCount + completedFlowCount) * 100.0 /
			(double)(currentLevels[currentLevelIndex].size * currentLevels[currentLevelIndex].size - currentLevels[currentLevelIndex].flowCount)));
		r.x = margin + 100;
		r.y = LEVEL_TILE_MARGIN_TOP - textH - 10;
		DrawString(screen, r, fontSmall, str, white, black);
		//currentLevels state
		r.x = screen->w - cMarkPic->w - 10;
		r.y = 30;
		switch (currentLevels[currentLevelIndex].state)
		{
		case Completed:
			SDL_BlitSurface(cMarkPic, 0, screen, &r);
			break;
		case Starred:
			SDL_BlitSurface(starPic, 0, screen, &r);
			break;
		default:
			break;
		}
		//horizontal grid
		for (i = 0; i <= currentLevels[currentLevelIndex].size; i++)
		{
			r.x = margin - GAME_AREA_GRID_WIDTH;
			r.w = GAME_AREA_GRID_WIDTH + currentLevels[currentLevelIndex].size *
				((GAME_AREA_SIZE - (currentLevels[currentLevelIndex].size - 1) *
				GAME_AREA_GRID_WIDTH) / currentLevels[currentLevelIndex].size + GAME_AREA_GRID_WIDTH) - 1;
			r.y = LEVEL_TILE_MARGIN_TOP - GAME_AREA_GRID_WIDTH + 
				i * ((GAME_AREA_SIZE - (currentLevels[currentLevelIndex].size - 1) *
				GAME_AREA_GRID_WIDTH) / currentLevels[currentLevelIndex].size + GAME_AREA_GRID_WIDTH);
			r.h = GAME_AREA_GRID_WIDTH - 1;
			boxColor(screen, r.x, r.y, r.x + r.w, r.y + r.h, SDLColorTo32bit(GAME_AREA_GRID_COLOR));
		}
		//vertical grid
		for (i = 0; i <= currentLevels[currentLevelIndex].size; i++)
		{
			r.x = margin - GAME_AREA_GRID_WIDTH +
				i * ((GAME_AREA_SIZE - (currentLevels[currentLevelIndex].size - 1) *
				GAME_AREA_GRID_WIDTH) / currentLevels[currentLevelIndex].size + GAME_AREA_GRID_WIDTH);
			r.w = GAME_AREA_GRID_WIDTH - 1;
			r.y = LEVEL_TILE_MARGIN_TOP - GAME_AREA_GRID_WIDTH;
			r.h = GAME_AREA_SIZE + GAME_AREA_GRID_WIDTH - 1;
			boxColor(screen, r.x, r.y, r.x + r.w, r.y + r.h, SDLColorTo32bit(GAME_AREA_GRID_COLOR));
		}
		//Flows
		r.w = (GAME_AREA_SIZE - (currentLevels[currentLevelIndex].size - 1) * GAME_AREA_GRID_WIDTH) / currentLevels[currentLevelIndex].size - 1;
		r.h = r.w;
		i = r.w * FLOW_SIZE_PERCENT / 100.0; //current flow width
		j = r.w * FLOW_END_SIZE_PERCENT / 100.0; //current flow end width
		for (k = 0; k < currentLevels[currentLevelIndex].flowCount; k++)
		{
			f = &currentLevels[currentLevelIndex].flows[k];
			FOR_EACH(fElem1, currentLevels[currentLevelIndex].flows[k].firstElement)
			{
				r.x = fElem1->position.x * (r.w + GAME_AREA_GRID_WIDTH + 1) + margin;
				r.y = fElem1->position.y * (r.w + GAME_AREA_GRID_WIDTH + 1) + LEVEL_TILE_MARGIN_TOP;
				boxColor(screen, r.x, r.y, r.x + r.w, r.y + r.h, SetOpacity(SDLColorTo32bit(f->color), FLOW_BG_OPACITY));
				if (fElem1->shape & UpS)
				{
					boxColor(screen, r.x+(r.w - i) / 2, r.y, r.x + (r.w - i) / 2 + i,
						r.y +(r.h - i) / 2 + i, SDLColorTo32bit(f->color));
				}
				//overlap grid
				if (fElem1->shape & DownS)
				{
					boxColor(screen, r.x + (r.w - i) / 2, r.y + r.w + GAME_AREA_GRID_WIDTH,
						r.x+(r.w - i) / 2 + i, r.y + (r.h - i) / 2, SDLColorTo32bit(f->color));
				}
				//overlap grid
				if (fElem1->shape & RightS)
				{
					boxColor(screen, r.x + (r.w - i) / 2, r.y +(r.h - i) / 2 + i, r.x + r.w +
						GAME_AREA_GRID_WIDTH, r.y + (r.h - i) / 2, SDLColorTo32bit(f->color));
				}
				if (fElem1->shape & LeftS)
				{
					boxColor(screen, r.x, r.y +(r.h - i)/2, r.x+(r.w - i) / 2 + i,
						r.y +(r.h - i) / 2 + i, SDLColorTo32bit(f->color));
				}
				if (fElem1->shape & EndS)
				{
					boxColor(screen, r.x + (r.w - j) / 2, r.y +(r.h - j) / 2, r.x+
						(r.w - j) / 2 + j, r.y +(r.h - j) / 2 +
						j, SDLColorTo32bit(f->color));
				}
			}
		}
		//print level name
		*str = 0;
		sprintf(str, "level %d", currentLevelIndex + 1);
		r.x = 50;
		r.y = 0;
		DrawString(screen, r, fontNormal, str, white, black);
		break;

	case GameOver:
		//blur previous screen
		if (!screenBlurred)
		{
			BlurH(screen, 2);
			BlurW(screen, 2);
			screenBlurred = 1;
		}
		r.x = menuButton.position.x;
		r.y = menuButton.position.y;
		SDL_BlitSurface(menuButton.picture, 0, screen, &r);
		if (!isTimeTrialGame)
		{
			if (currentLevels[currentLevelIndex].state != Starred)
			{
				TTF_SizeText(fontSmall, "fill the whole game area.", &textW, &textH);
				r.y = arrowNext.position.y - arrowNext.picture->h - textH;
				r.x = screen->w / 2 - textW / 2;
				DrawString(screen, r, fontSmall, "fill the whole game area.", white, black);
				r.y -= textH;
				TTF_SizeText(fontSmall, "If you want to get a star,", &textW, &textH);
				r.x = screen->w / 2 - textW / 2;
				DrawString(screen, r, fontSmall, "If you want to get a star,", white, black);
				TTF_SizeText(fontNormal, "completed", &textW, &textH);
				r.x = screen->w / 2 - textW / 2;
				r.y -= textH;
				DrawString(screen, r, fontNormal, "completed", white, black);
				//reload currentLevels button
				r.x = reload.position.x;
				r.y = reload.position.y;
				SDL_BlitSurface(reload.picture, 0, screen, &r);
			}
			else
			{
				TTF_SizeText(fontNormal, "completed", &textW, &textH);
				r.x = screen->w / 2 - textW / 2;
				r.y = arrowNext.position.y - arrowNext.picture->h - textH;
				DrawString(screen, r, fontNormal, "completed", white, black);
			}
			if (currentLevelIndex != currentLevelCount - 1)
			{
				r.x = arrowNext.position.x;
				r.y = arrowNext.position.y;
				SDL_BlitSurface(arrowNext.picture, 0, screen, &r);
			}
		}
		else
		{
			TTF_SizeText(fontNormal, "game over", &textW, &textH);
			r.x = (screen->w - textW) / 2;
			r.y = arrowNext.position.y - arrowNext.picture->h - textH;
			DrawString(screen, r, fontNormal, "game over", white, black);
			//score
			*str = 0;
			sprintf(str, "you have made %d levels", currentTimeTScore);
			TTF_SizeText(fontSmall, str, &textW, NULL);
			r.x = screen->w / 2 - textW / 2;
			r.y += textH;
			DrawString(screen, r, fontSmall, str, white, black);
		}
		break;

	case UserLevelLoading:
		if (!screenBlurred)
		{
			BlurH(screen, 2);
			BlurW(screen, 2);
			screenBlurred = 1;
		}
		r.x = menuButton.position.x;
		r.y = menuButton.position.y;
		SDL_BlitSurface(menuButton.picture, 0, screen, &r);
		TTF_SizeText(fontSmall, userLevelError, &textW, &textH);
		r.y = arrowNext.position.y - arrowNext.picture->h - textH;
		r.x = screen->w / 2 - textW / 2;
		DrawString(screen, r, fontSmall, userLevelError, white, black);
		break;

	case TimeTrialMenu:
		SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));
		r.x = arrowBack.position.x;
		r.y = arrowBack.position.y;
		SDL_BlitSurface(arrowBack.picture, 0, screen, &r); r.x = 50;
		r.y = 0;
		DrawString(screen, r, fontNormal, "time trial", white, black);
		r.y = MAINMENU_ITEM_MARGIN;
		r.x = TIME_TRIAL_MARGIN_LEFT;
		for (i = 0; i < sizeof(timeTrialMenuItems)/sizeof(TimeTrialMenuItem); i++)
		{
			TTF_SizeText(fontNormal, timeTrialMenuItems[i].name, NULL, &textH);
			if (timeTrialMenuItems[i].mouseOver)
			{
				if (timeTrialMenuItems[i].mouseDown)
					DrawString(screen, r, fontNormal, timeTrialMenuItems[i].name,
					Darken(timeTrialMenuItems[i].color, 0.3), timeTrialMenuItems[i].color);
				else
					DrawString(screen, r, fontNormal, timeTrialMenuItems[i].name,
					Darken(timeTrialMenuItems[i].color, 0.3), black);
			}
			else
				DrawString(screen, r, fontNormal, timeTrialMenuItems[i].name,
				timeTrialMenuItems[i].color, black);
			if (timeTHighScores[timeTrialMenuItems[i].index] != 0)
			{
				*str = 0;
				sprintf(str, "%d", timeTHighScores[timeTrialMenuItems[i].index]);
				TTF_SizeText(fontNormal, str, &textW, NULL);
				r.x = screen->w - textW - TIME_TRIAL_MARGIN_LEFT;
				DrawString(screen, r, fontNormal, str, white, black);
				r.x = TIME_TRIAL_MARGIN_LEFT;
			}
			r.y += textH;
		}
		break;

	case AboutMenu:
		SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));
		r.x = arrowBack.position.x;
		r.y = arrowBack.position.y;
		SDL_BlitSurface(arrowBack.picture, 0, screen, &r);
		r.x = 50;
		r.y = 0;
		DrawString(screen, r, fontNormal, "about", white, black);
		TTF_SizeText(fontNormal, "szabolevente", &textW, NULL);
		r.x = screen->w / 2 - textW / 2;
		r.y = 200;
		aboutAnimation += 0xc3a3;
		white.r = (aboutAnimation & 0xff0000) >> 16;
		white.g = (aboutAnimation & 0xff00) >> 8;
		white.b = (aboutAnimation & 0xff);
		DrawString(screen, r, fontNormal, "szabolevente", white, black);
		TTF_SizeText(fontNormal, "@ mail.com", &textW, &textH);
		r.x = screen->w / 2 - textW / 2;
		r.y += textH;
		DrawString(screen, r, fontNormal, "@ mail.com", white, black);
		white.r = 255;
		white.g = 255;
		white.b = 255;
		break;
	default:
		break;
	}
	SDL_Flip(screen);
}

/// <summary>
/// Sends a user event.
/// </summary>
/// <param name="ms">The previous wait time in milliseconds.</param>
/// <param name="param">User definied param.</param>
/// <returns>Time to wait until next call in millisecons.</returns>
Uint32 SendUserEventTick(Uint32 ms, void* param)
{
	SDL_Event ev;
	ev.type = SDL_USEREVENT;
	SDL_PushEvent(&ev);
	return ms;
}

/// <summary>
/// Loads the game resources.
/// </summary>
/// <returns>Returns 0 if succeess, 1 if error</returns>
int LoadResources()
{
	FILE *file;
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
	{
		printf( "Unable to init SDL: %s\n", SDL_GetError());
		return -1;
	}
	atexit(SDL_Quit);
	if(TTF_Init()==-1)
	{
		printf("Unable to init TTF: %s\n", TTF_GetError());
		return -1;
	}
	atexit(TTF_Quit);
	if ((screen = SDL_SetVideoMode(480, 640, 0, SDL_HWSURFACE|SDL_DOUBLEBUF)) == NULL)
	{
		printf("Unable to set 480x640 video: %s\n", SDL_GetError());
		return 1;
	}
	if(IMG_Init(IMG_INIT_TIF | IMG_INIT_JPG | IMG_INIT_PNG) == 0)
	{
		printf("Failed to init required support!\n");
	}
	atexit(IMG_Quit);
	if ((fontTitle = TTF_OpenFont("DunkinSans.ttf", FONTSIZE_BIG)) == NULL)
	{
		printf("Unable to open font.\n");
		return -1;
	}
	if ((fontNormal = TTF_OpenFont("DunkinSans.ttf", FONTSIZE_NORMAL)) == NULL)
	{
		printf("Unable to open font.\n");
		return -1;
	}
	if ((fontSmall = TTF_OpenFont("DunkinSans.ttf", FONTSIZE_SMALL)) == NULL)
	{
		printf("Unable to open font.\n");
		return -1;
	}
	if ((icon = IMG_Load("icon.bmp")) == NULL)
	{
		printf("Unable to load bitmap: %s\n", SDL_GetError());
		return -1;
	}
	if ((arrowBack.pictureDefault = IMG_Load("left_arrow.png")) == NULL)
	{
		printf("Unable to load bitmap: %s\n", SDL_GetError());
		return -1;
	}
	if ((arrowBack.pictureMouseOver = IMG_Load("left_arrow_clicked.png")) == NULL)
	{
		printf("Unable to load bitmap: %s\n", SDL_GetError());
		return -1;
	}
	if ((starPic = IMG_Load("star.png")) == NULL)
	{
		printf("Unable to load bitmap: %s\n", SDL_GetError());
		return -1;
	}
	if ((cMarkPic = IMG_Load("cMark.png")) == NULL)
	{
		printf("Unable to load bitmap: %s\n", SDL_GetError());
		return -1;
	}
	if ((menuButton.pictureDefault = IMG_Load("menu.png")) == NULL)
	{
		printf("Unable to load bitmap: %s\n", SDL_GetError());
		return -1;
	}
	if ((menuButton.pictureMouseOver = IMG_Load("menu_clicked.png")) == NULL)
	{
		printf("Unable to load bitmap: %s\n", SDL_GetError());
		return -1;
	}
	if ((reload.pictureDefault = IMG_Load("reload.png")) == NULL)
	{
		printf("Unable to load bitmap: %s\n", SDL_GetError());
		return -1;
	}
	if ((reload.pictureMouseOver = IMG_Load("reload_clicked.png")) == NULL)
	{
		printf("Unable to load bitmap: %s\n", SDL_GetError());
		return -1;
	}
	//Load default currentLevels
	if ((file = fopen("defaultLevels.txt", "rt")) == NULL)
	{
		printf("Unable to load currentLevels\n");
		return -1;
	}
	defaultLevels = NULL;
	LoadLevelsFromFile(file, &defaultLevels, &defaultLevelCount);
	if (!defaultLevels)
	{
		printf("Unable to parse currentLevels\n");
		fclose(file);
		return -1;
	}
	fclose(file);
	//load time trial high scores
	timeTHighScores[0] = 0;
	timeTHighScores[1] = 0;
	timeTHighScores[2] = 0;
	if ((file = fopen("scores.txt", "rt")) != NULL)
	{
		fscanf(file, "%d,", &timeTHighScores[0]);
		fscanf(file, "%d,", &timeTHighScores[1]);
		fscanf(file, "%d", &timeTHighScores[2]);
		fclose(file);
	}
	return 0;
}

/// <summary>
/// Unloads the game resources.
/// </summary>
void UnloadResources()
{
	int i;
	SDL_RemoveTimer(userTimer);
	SDL_FreeSurface(starPic);
	SDL_FreeSurface(cMarkPic);
	SDL_FreeSurface(screen);
	TTF_CloseFont(fontTitle);
	TTF_CloseFont(fontNormal);
	TTF_CloseFont(fontSmall);
	if (userLevels)
	{
		for (i = 0; i < userLevelCount; i++)
			FreeLevelContent(&userLevels[i]);
		free(userLevels);
	}
	if (defaultLevels)
	{
		for (i = 0; i < defaultLevelCount; i++)
			FreeLevelContent(&defaultLevels[i]);
		free(defaultLevels);
	}
}

/// <summary>
/// Initialize game.
/// </summary>
/// <returns></returns>
int Init()
{
	SDL_WM_SetCaption("Flow", "Flow");
	SDL_WM_SetIcon(icon, NULL);
	srand(SDL_GetTicks());

	//Game vars
	gameState = MainMenu;
	exiting = 0;
	loadUserLevel = 1;
	currentLevels = defaultLevels;
	userLevels = NULL;
	currentLevelCount = defaultLevelCount;
	currentLevelIndex = 0;
	currentLevelSelectPage = 0;
	levelSelectPageCount = 1 + (defaultLevelCount - 1 ) / 9;
	aboutAnimation = 0;
	isTimeTrialGame = 0;
	currentTime = SDL_GetTicks();

	//set button pictures
	if ((arrowNext.pictureDefault = FlipH(arrowBack.pictureDefault)) == NULL)
	{
		printf("Unable to flip bitmap\n");
		return -1;
	}
	if ((arrowNext.pictureMouseOver = FlipH(arrowBack.pictureMouseOver)) == NULL)
	{
		printf("Unable to flip bitmap\n");
		return -1;
	}
	arrowPrev.pictureDefault = arrowBack.pictureDefault;
	arrowPrev.pictureMouseOver = arrowBack.pictureMouseOver;
	reload.picture = reload.pictureDefault;
	arrowBack.picture = arrowBack.pictureDefault;
	arrowNext.picture = arrowNext.pictureDefault;
	arrowPrev.picture = arrowPrev.pictureDefault;
	menuButton.picture = menuButton.pictureDefault;

	//set button positions
	arrowNext.position.x = screen->w / 2 - (int)(arrowNext.picture->w * 0.5) + LEVEL_CHANGE_ARROW_DIST;
	arrowPrev.position.x = screen->w / 2 - (int)(arrowPrev.picture->w * 0.5) - LEVEL_CHANGE_ARROW_DIST;
	menuButton.position.x = screen->w / 2 - menuButton.picture->w * 0.5;
	return 0;
}

/// <summary>
/// Saves the loaded levels to file.
/// </summary>
void Save()
{
	FILE *file;
	int i, j;
	if ((file = fopen("defaultLevels.txt", "wt")) != NULL)
	{
		for (i = 0; i < defaultLevelCount; i++)
		{
			fprintf(file, "{");
			for (j = 0; j < defaultLevels[i].flowCount; j++)
			{
				fprintf(file, "{%d,%d,%d,%d},",
					defaultLevels[i].flows[j].firstElement->position.x + 1,
					defaultLevels[i].flows[j].firstElement->position.y + 1,
					defaultLevels[i].flows[j].lastElement->position.x + 1,
					defaultLevels[i].flows[j].lastElement->position.y + 1);
			}
			if (i == defaultLevelCount - 1)
				fprintf(file, "%d,%d,0}", defaultLevels[i].size, defaultLevels[i].state);
			else
				fprintf(file, "%d,%d,0}\n", defaultLevels[i].size, defaultLevels[i].state);
		}
		fclose(file);
	}
	if ((file = fopen("scores.txt", "wt")) != NULL)
	{
		fprintf(file, "%d,%d,%d", timeTHighScores[0], timeTHighScores[1], timeTHighScores[2]);
		fclose(file);
	}
}

int main(int argc, char* argv[])
{
	if(LoadResources() == -1)
		return 1;
	atexit(UnloadResources);
	if(Init() == -1)
		return 1;

	//Main game loop
	userTimer = SDL_AddTimer(400, SendUserEventTick, NULL);
	while (!exiting)
	{
		ProcessEvents();
		if (Update() == -1)
			return 1;
		Draw();
	}
	Save();
	return 0;
}