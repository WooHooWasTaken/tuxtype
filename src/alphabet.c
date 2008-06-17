/***************************************************************************
                          alphabet.c 
 -  description: Init SDL
                             -------------------
    begin                : Jan 6 2003
    copyright            : (C) 2003 by Jesse Andrews
    email                : jdandr2@tux4kids.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


/* Needed to handle rendering issues for Indic languages*/
#ifndef WIN32
#ifndef MACOSX
#include <SDL_Pango.h>
#endif
#endif

/* Needed to convert UTF-8 under Windows because we don't have glibc: */
#include "ConvertUTF.h"  

#include "globals.h"
#include "funcs.h"


/* NOTE these are externed in globals.h so not static */
/* the colors we use throughout the game */
SDL_Color black;
SDL_Color gray;
SDL_Color dark_blue;
SDL_Color red;
SDL_Color white;
SDL_Color yellow;



/* An individual item in the list of cached unicode characters that are rendered at   */
/* the start of each game.                                                            */
typedef struct uni_glyph {
  wchar_t unicode_value;
  SDL_Surface* white_glyph;
  SDL_Surface* red_glyph;
} uni_glyph;

/* These are the arrays for the red and white letters: */
static uni_glyph char_glyphs[MAX_UNICODES] = {0, NULL, NULL};

/* An individual item in the list of unicode characters in the keyboard setup.   */
/* Basically, just the Unicode value for the key and the finger used to type it. */
typedef struct kbd_char {
  wchar_t unicode_value;
  char finger;
} kbd_char;

/* List with one entry for each typable character in keyboard setup - has the */
/* Unicode value of the key and the associated fingering.                     */
static kbd_char keyboard_list[MAX_UNICODES] = {0, -1};



static TTF_Font* font = NULL;

/* Used for word list functions (see below): */
static int num_words;
static wchar_t word_list[MAX_NUM_WORDS][MAX_WORD_SIZE + 1];
static wchar_t char_list[MAX_UNICODES];  // List of distinct letters in word list
static int num_chars_used = 0;       // Number of different letters in word list



/* Local function prototypes: */
static void gen_char_list(void);
static int add_char(wchar_t uc);
static void set_letters(unsigned char* t);
static void show_letters(void);
static void clear_keyboard(void);
static int unicode_in_key_list(wchar_t uni_char);
int check_needed_unicodes_str(const wchar_t* s);

#ifndef WIN32
#ifndef MACOSX
static SDLPango_Matrix* SDL_Colour_to_SDLPango_Matrix(const SDL_Color* cl);
#endif
#endif



/*****************************************************/
/*                                                   */
/*          "Public" Functions                       */
/*                                                   */
/*****************************************************/



/* FIXME would be better to get keymap from system somehow (SDL? X11?) - */
/* all this does now is fiddle with the ALPHABET and FINGER arrays */
int LoadKeyboard(void)
{
  unsigned char fn[FNLEN];
  int found = 0;

  clear_keyboard();

  /* First look for keyboard.lst in theme path, if desired: */
  if (!settings.use_english)
  {
    sprintf(fn , "%s/keyboard.lst", settings.theme_data_path);
    if (CheckFile(fn))
    {
      found = 1;
    }
  }

  /* Now look in default path if desired or needed: */
  if (!found)
  {
    sprintf(fn , "%s/keyboard.lst", settings.default_data_path);
    if (CheckFile(fn))
    {
      found = 1;
    }
  }

  if (!found)
  {
    fprintf(stderr, "LoadKeyboard(): Error finding file for keyboard setup!\n");
    return 0;
  }
  
  /* fn should now contain valid path to keyboard.lst: */
  DEBUGCODE{fprintf(stderr, "fn = %s\n", fn);}

  {
    unsigned char str[255];
    wchar_t wide_str[255];

    FILE* f;
    int i = 0, j = 0, k = 0;

    f = fopen( fn, "r" );

    if (f == NULL)
    {
      LOG("LoadKeyboard() - could not open keyboard.lst\n");
      return 0;
    }


    do
    {
      fscanf( f, "%[^\n]\n", str);
      /* Convert to wcs from UTF-8, if needed; */
      //mbstowcs(wide_str, str, strlen(str) + 1);
      ConvertFromUTF8(wide_str, str);

      /* Line must have 3 chars (if more, rest are ignored) */
      /* Format is: FINGER|Char  e.g   "3|d"                */
      /* wide_str[0] == finger used to type char            */
      /* wide_str[1] =='|'
      /* wide_str[2] == Unicode value of character          */

      /* FIXME - this might be a good place to use a    */
      /* hash table to avoid n^2 performance problems.  */
      /* Some sanity checks:  */
      if ((wcslen(wide_str) >=3)
       && (wcstol(&wide_str[0], NULL, 0) >=0)   /* These lines just make sure the */
       && (wcstol(&wide_str[0], NULL, 0) < 10)  /* finger is between 0 and 10     */
       && (wide_str[1] == '|')
       && (k < MAX_UNICODES)
       && !unicode_in_key_list(wide_str[2])) /* Make sure char not already added */
      {
        DEBUGCODE
        {
          fprintf(stderr, "Adding key: Unicode char = '%C'\tUnicode value = %d\tfinger = %d\n",
                  wide_str[2], wide_str[2], wcstol(&wide_str[0], NULL, 0)); 
        }

        /* Just plug values into array: */
        keyboard_list[k].unicode_value = wide_str[2];
        keyboard_list[k].finger = wcstol(&wide_str[0], NULL, 0);
        k++;
      }
    } while (!feof(f));


    fclose(f);

    LOG("Leaving LoadKeyboard()\n");
    return 1;
  }
}

/* Returns the finger hint(0-9) associated with a given Unicode value */
/* in the keyboard_list:                                              */
/* Returns -1 if somehow no finger associated with a Unicode value    */
/* in the list (shouldn't happen).                                    */
/* Returns -2 if Unicode value not in list.                           */
int GetFinger(wchar_t uni_char)
{
  int i = 0;

  while ((i < MAX_UNICODES)
     &&  (keyboard_list[i].unicode_value != uni_char))
  {
    i++;
  }

  if (i == MAX_UNICODES)
  {
    fprintf(stderr, "GetFinger() - Unicode char '%C' not found in list.\n", uni_char);
    return -2;
  }

  if ((keyboard_list[i].finger < 0)
   || (keyboard_list[i].finger > 9))
  {
    fprintf(stderr, "GetFinger() - Unicode char '%C' has no valid finger.\n",uni_char);
    return -1;
  }  

  return (int)keyboard_list[i].finger; /* Keep compiler from complaining */
}



int unicode_in_key_list(wchar_t uni_char)
{
  int i = 0;
  while ((i < MAX_UNICODES)
     &&  (keyboard_list[i].unicode_value != uni_char))
  {
    i++;
  }
  if (i == MAX_UNICODES)
    return 0;
  else
    return 1;
}

/* NOTE if we can consistently use SDLPango on all platforms, we can simply */
/* rename the pango version to BlackOutline() and get rid of this one.      */
/* The input for this function should be UTF-8.                             */
SDL_Surface* BlackOutline(const unsigned char* t, const TTF_Font* font, const SDL_Color* c)
{
  SDL_Surface* out = NULL;
  SDL_Surface* black_letters = NULL;
  SDL_Surface* white_letters = NULL;
  SDL_Surface* bg = NULL;
  SDL_Rect dstrect;
  Uint32 color_key;

  LOG("Entering BlackOutline()\n");

/* Simply passthrough to SDLPango version if available (i.e. not under Windows):*/
#ifndef WIN32
#ifndef MACOSX
return BlackOutline_SDLPango(t, font, c);
#endif
#endif


  if (!t || !font || !c)
  {
    fprintf(stderr, "BlackOutline(): invalid ptr parameter, returning.");
    return NULL;
  }

  black_letters = TTF_RenderUTF8_Blended((TTF_Font*)font, t, black);

  if (!black_letters)
  {
    fprintf (stderr, "Warning - BlackOutline() could not create image for %s\n", t);
    return NULL;
  }

  bg = SDL_CreateRGBSurface(SDL_SWSURFACE,
                            (black_letters->w) + 5,
                            (black_letters->h) + 5,
                             32,
                             RMASK, GMASK, BMASK, AMASK);
  /* Use color key for eventual transparency: */
  color_key = SDL_MapRGB(bg->format, 10, 10, 10);
  SDL_FillRect(bg, NULL, color_key);

  /* Now draw black outline/shadow 2 pixels on each side: */
  dstrect.w = black_letters->w;
  dstrect.h = black_letters->h;

  /* NOTE: can make the "shadow" more or less pronounced by */
  /* changing the parameters of these loops.                */
  for (dstrect.x = 1; dstrect.x < 4; dstrect.x++)
    for (dstrect.y = 1; dstrect.y < 3; dstrect.y++)
      SDL_BlitSurface(black_letters , NULL, bg, &dstrect );

  SDL_FreeSurface(black_letters);

  /* --- Put the color version of the text on top! --- */
  /* NOTE we cast away the 'const-ness' to keep compliler from complaining: */
  white_letters = TTF_RenderUTF8_Blended((TTF_Font*)font, t, *c);
  dstrect.x = 1;
  dstrect.y = 1;
  SDL_BlitSurface(white_letters, NULL, bg, &dstrect);
  SDL_FreeSurface(white_letters);

  /* --- Convert to the screen format for quicker blits --- */
  SDL_SetColorKey(bg, SDL_SRCCOLORKEY|SDL_RLEACCEL, color_key);
  out = SDL_DisplayFormatAlpha(bg);
  SDL_FreeSurface(bg);

  LOG("Leaving BlackOutline()\n");

  return out;
}



#ifndef WIN32
#ifndef MACOSX
/*Convert SDL_Colour to SDLPango_Matrix*/

SDLPango_Matrix* SDL_Colour_to_SDLPango_Matrix(const SDL_Color *cl)
{
  SDLPango_Matrix *colour;
  colour=malloc(sizeof(SDLPango_Matrix));
  int k;
  for(k=0;k<4;k++){
  	(*colour).m[0][k]=(*cl).r;
  	(*colour).m[1][k]=(*cl).g;
  	(*colour).m[2][k]=(*cl).b;
  }
  (*colour).m[3][0]=0;
  (*colour).m[3][1]=255;
  (*colour).m[3][2]=0;
  (*colour).m[3][3]=0;

  return colour;
}



/* This version basically uses the SDLPango lib instead of */
/* TTF_RenderUTF*_Blended() to properly render Indic text. */
SDL_Surface* BlackOutline_SDLPango(const unsigned char* t, const TTF_Font* font, const SDL_Color* c)
{
  SDL_Surface* out = NULL;
  SDL_Surface* black_letters = NULL;
  SDL_Surface* white_letters = NULL;
  SDL_Surface* bg = NULL;
  SDL_Rect dstrect;
  Uint32 color_key;
  /* To covert SDL_Colour to SDLPango_Matrix */
  SDLPango_Matrix* colour = NULL;
  /* Create a context which contains Pango objects.*/
  SDLPango_Context* context = NULL;

  LOG("\nEntering BlackOutline_SDLPango()\n");
  DEBUGCODE{ fprintf(stderr, "will attempt to render: %s\n", t); }

  if (!t || !font || !c)
  {
    fprintf(stderr, "BlackOutline_SDLPango(): invalid ptr parameter, returning.");
    return NULL;
  }

  /* SDLPango crashes on 64 bit machines if passed empty string - Debian Bug#439071 */
  if (*t == '\0')
  {
    fprintf(stderr, "BlackOutline_SDLPango(): empty string arg - must return to avoid segfault.");
    return NULL;
  }

  colour = SDL_Colour_to_SDLPango_Matrix(c);
  
  /* Create the context */
  context = SDLPango_CreateContext();	
  SDLPango_SetDpi(context, 125.0, 125.0);
  /* Set the color */
  SDLPango_SetDefaultColor(context, MATRIX_TRANSPARENT_BACK_BLACK_LETTER );
  SDLPango_SetBaseDirection(context, SDLPANGO_DIRECTION_LTR);
  /* Set text to context */ 
  SDLPango_SetMarkup(context, t, -1); 

  if (!context)
  {
    fprintf (stderr, "In BlackOutline_SDLPango(), could not create context for %s", t);
    return NULL;
  }

  black_letters = SDLPango_CreateSurfaceDraw(context);

  if (!black_letters)
  {
    fprintf (stderr, "Warning - BlackOutline_SDLPango() could not create image for %s\n", t);
    return NULL;
  }

  bg = SDL_CreateRGBSurface(SDL_SWSURFACE,
                            (black_letters->w) + 5,
                            (black_letters->h) + 5,
                             32,
                             RMASK, GMASK, BMASK, AMASK);
  if (!bg)
  {
    fprintf (stderr, "Warning - BlackOutline()_SDLPango - bg creation failed\n");
    SDL_FreeSurface(black_letters);
    return NULL;
  }

  /* Draw text on a existing surface */
  SDLPango_Draw(context, bg, 0, 0);

  /* Use color key for eventual transparency: */
  color_key = SDL_MapRGB(bg->format, 10, 10, 10);
  SDL_FillRect(bg, NULL, color_key);

  /* Now draw black outline/shadow 2 pixels on each side: */
  dstrect.w = black_letters->w;
  dstrect.h = black_letters->h;

  /* NOTE: can make the "shadow" more or less pronounced by */
  /* changing the parameters of these loops.                */
  for (dstrect.x = 1; dstrect.x < 4; dstrect.x++)
    for (dstrect.y = 1; dstrect.y < 3; dstrect.y++)
      SDL_BlitSurface(black_letters , NULL, bg, &dstrect );

  SDL_FreeSurface(black_letters);

  /* --- Put the color version of the text on top! --- */
  SDLPango_SetDefaultColor(context, colour);
  white_letters = SDLPango_CreateSurfaceDraw(context);
  dstrect.x = 1;
  dstrect.y = 1;
  SDL_BlitSurface(white_letters, NULL, bg, &dstrect);
  SDL_FreeSurface(white_letters);

  /* --- Convert to the screen format for quicker blits --- */
  SDL_SetColorKey(bg, SDL_SRCCOLORKEY|SDL_RLEACCEL, color_key);
  out = SDL_DisplayFormatAlpha(bg);
  SDL_FreeSurface(bg);

  LOG("Leaving BlackOutline_SDLPango()\n\n");

  return out;
}

#endif
#endif
/* End of win32-excluded coded */




/* This version takes a wide character string and renders it with the */
/* Unicode string versions of the SDL_ttf functions:                  */
SDL_Surface* BlackOutline_Unicode(const Uint16* t, const TTF_Font* font, const SDL_Color* c)
{
  SDL_Surface* out = NULL;
  SDL_Surface* black_letters = NULL;
  SDL_Surface* white_letters = NULL;
  SDL_Surface* bg = NULL;
  SDL_Rect dstrect;
  Uint32 color_key;

  if (!font || !c)
  {
    fprintf(stderr, "BlackOutline_wchar(): invalid ptr parameter, returning.");
    return NULL;
  }
                                        /* (cast to stop compiler complaint) */
  black_letters = TTF_RenderUNICODE_Blended((TTF_Font*)font, t, black);

  if (!black_letters)
  {
    fprintf (stderr, "Warning - BlackOutline_wchar() could not create image for %S\n", t);
    return NULL;
  }

  bg = SDL_CreateRGBSurface(SDL_SWSURFACE,
                            (black_letters->w) + 5,
                            (black_letters->h) + 5,
                             32,
                             RMASK, GMASK, BMASK, AMASK);
  /* Use color key for eventual transparency: */
  color_key = SDL_MapRGB(bg->format, 10, 10, 10);
  SDL_FillRect(bg, NULL, color_key);

  /* Now draw black outline/shadow 2 pixels on each side: */
  dstrect.w = black_letters->w;
  dstrect.h = black_letters->h;

  /* NOTE: can make the "shadow" more or less pronounced by */
  /* changing the parameters of these loops.                */
  for (dstrect.x = 1; dstrect.x < 4; dstrect.x++)
    for (dstrect.y = 1; dstrect.y < 3; dstrect.y++)
      SDL_BlitSurface(black_letters , NULL, bg, &dstrect );

  SDL_FreeSurface(black_letters);

  /* --- Put the color version of the text on top! --- */
                                       /* (cast to stop compiler complaint) */
  white_letters = TTF_RenderUNICODE_Blended((TTF_Font*)font, t, *c);
  dstrect.x = 1;
  dstrect.y = 1;
  SDL_BlitSurface(white_letters, NULL, bg, &dstrect);
  SDL_FreeSurface(white_letters);

  /* --- Convert to the screen format for quicker blits --- */
  SDL_SetColorKey(bg, SDL_SRCCOLORKEY|SDL_RLEACCEL, color_key);
  out = SDL_DisplayFormatAlpha(bg);
  SDL_FreeSurface(bg);

  return out;
}


/* FIXME dead code but could be useful*/
static void show_letters(void)
{
	int i, l = 0;
	SDL_Surface* abit;
	SDL_Rect dst;
	int stop = 0;
	unsigned char t[255];

	for (i=0; i<256; i++)
		if (ALPHABET[i])
			t[l++]=i;

	t[l] = 0;

	abit = BlackOutline(t, font, &white);

	dst.x = 320 - (abit->w / 2);
	dst.y = 275;
	dst.w = abit->w;
	dst.h = abit->h;

	SDL_BlitSurface(abit, NULL, screen, &dst);

	SDL_FreeSurface(abit);

	abit = BlackOutline("Alphabet Set To:", font, &white);
	dst.x = 320 - (abit->w / 2);
	dst.y = 200;
	dst.w = abit->w;
	dst.h = abit->h;

	SDL_BlitSurface(abit, NULL, screen, &dst);

	SDL_UpdateRect(screen, 0, 0, 0 ,0);

	while (!stop) 
		while (SDL_PollEvent(&event)) 
			switch (event.type) {
				case SDL_QUIT:
					exit(0);
				case SDL_KEYDOWN:
				case SDL_MOUSEBUTTONDOWN:
					stop = 1;
			}

	SDL_FreeSurface(abit);
}


/* Returns a random Unicode char from the char_glyphs list: */
/* --- get a letter --- */
wchar_t GetRandLetter(void)
{
  static wchar_t last = -1; // we don't want to return same letter twice in a row
  wchar_t letter;
  int i = 0;

  if (!num_chars_used)
  {
    fprintf(stderr, "GetRandLetter() - no letters in list!\n");
    last = -1;
    return -1;
  }

  do
  {
    i = rand() % num_chars_used;
    letter = char_glyphs[i].unicode_value;
  } while (letter == last);

  last = letter;

  return letter;
}

/******************************************************************************
*                           WORD FILE & DATA STRUCTURE                        *
******************************************************************************/



/* ClearWordList: clears the number of words
 */
void ClearWordList(void)
{
  int i;
  for (i = 0; i < num_words; i++)
  {
    word_list[i][0] = '\0';
  }
  num_words = 0;
}

/* FIXME need a better i18n-compatible way to do this: */
/* UseAlphabet(): setups the word_list so that it really
 * returns a LETTER when GetWord() is called
 */
// void UseAlphabet(void)
// {
// 	int i;
// 
// 	LOG("Entering UseAlphabet()\n");
// 
// 	num_words = 0;
// 	/* This totally mucks up i18n abilities :( */
// 	for (i=65; i<90; i++) 
// 	{
// 		//if (ALPHABET[i])
//                 {
// 			word_list[num_words][0] = (unsigned char)i;
// 			word_list[num_words][1] = '\0';
// 			num_words++;
// 
// 			DEBUGCODE { fprintf(stderr, "Adding %c\n", (unsigned char)i); }
// 		}
// 	}
// 	/* Make sure list is terminated with null character */
// 	word_list[num_words][0] = '\0';
// 
// 	/* Make list of all unicode characters used in word list: */
// 	gen_char_list();
// 
// 	DOUT(num_words);
// 	LOG("Leaving UseAlphabet()\n");
// }

/* GetWord: returns a random word that wasn't returned
 * the previous time (unless there is only 1 word!!!)
 */
wchar_t* GetWord(void)
{
	static int last_choice = -1;
	int choice;

	LOG("Entering GetWord()\n");
	DEBUGCODE { fprintf(stderr, "num_words is: %d\n", num_words); }

	/* Now count list to make sure num_words is correct: */

	num_words = 0;
	while (word_list[num_words][0] != '\0')
	{
	  num_words++;
	}

	DEBUGCODE { fprintf(stderr, "After count, num_words is: %d\n", num_words); }

        if (0 == num_words)
	{
	  LOG("No words in list\n");
          return NULL;
	}

        if (num_words > MAX_NUM_WORDS)
	{
	  LOG("Error: num_words greater than array size\n");
          return NULL;
	}

        if (num_words < 0)
	{
	  LOG("Error: num_words negative\n");
          return NULL;
	}

	do {
		choice = (rand() % num_words);
	} while ((choice == last_choice) || (num_words < 2));

	last_choice = choice;

	/* NOTE need %S rather than %s because of wide characters */
	DEBUGCODE { fprintf(stderr, "Selected word is: %S\n", word_list[choice]); }

	return word_list[choice];
}



/* GenerateWordList(): adds the words from a given wordlist
 * it ignores any words too long or that has bad
 * character (such as #)
 */

/* Now returns the number of words in the list, so if no words */
/* returns "false"                                             */
int GenerateWordList(const char* wordFn)
{
  int j;
  unsigned char temp_word[FNLEN];
  wchar_t temp_wide_word[FNLEN];
  size_t length;

  FILE* wordFile=NULL;

  DEBUGCODE { fprintf(stderr, "Entering GenerateWordList() for file: %s\n", wordFn); }

  num_words = 0;

  /* --- open the file --- */

  wordFile = fopen( wordFn, "r" );

  if ( wordFile == NULL )
  {
    fprintf(stderr, "ERROR: could not load wordlist: %s\n", wordFn );
//    UseAlphabet( );
    return 0;
  }


  /* --- load words from file named as argument: */

  DEBUGCODE { fprintf(stderr, "WORD FILE OPENNED @ %s\n", wordFn); }

  /* ignore the title (i.e. first line) */
  fscanf( wordFile, "%[^\n]\n", temp_word);

  while (!feof(wordFile) && (num_words < MAX_NUM_WORDS))
  {
    fscanf( wordFile, "%[^\n]\n", temp_word);
    DEBUGCODE {fprintf(stderr, "temp_word = %s\n", temp_word);}

    for (j = 0; j < strlen(temp_word); j++)
    {
      if (temp_word[j] == '\n' || temp_word[j] == '\r')
        temp_word[j] = '\0';
    }

    /* Convert from UTF-8 to wcs and make sure word is usable: */
    /* NOTE need to add one to length arg so terminating '\0' gets added: */
    //length = mbstowcs(temp_wide_word, temp_word, strlen(temp_word) + 1);

    length = ConvertFromUTF8(temp_wide_word, temp_word);
    DOUT(length);

    if (length == -1)  /* Means invalid UTF-8 sequence or conversion failed */
    {
      fprintf(stderr, "Word '%s' not added - invalid UTF-8 sequence!\n", temp_word);
      continue;
    }

    if (length == 0)
    {
      fprintf(stderr, "Word '%ls' not added - length is zero\n", temp_wide_word);
      continue;
    }

    if (length > MAX_WORD_SIZE)
    {
      fprintf(stderr, "Word '%s' not added - exceeds %d characters\n",
              temp_word, MAX_WORD_SIZE);
      continue;
    }

    if (num_words >= MAX_NUM_WORDS)
    {
      fprintf(stderr, "Word '%s' not added - list has reached max of %d characters\n",
              temp_word, MAX_NUM_WORDS);
      continue;
    }

    if (!check_needed_unicodes_str(temp_wide_word))
    {
      fprintf(stderr, "Word '%S' not added - contains Unicode chars not in keyboard list\n",
              temp_wide_word);
      continue;
    }

    /* If we make it to here, OK to add word: */
    /* NOTE we have to add one to the length argument */
    /* to include the terminating null.  */
    DEBUGCODE
    {
      fprintf(stderr, "Adding word: %ls\n", temp_wide_word);
    }

    wcsncpy(word_list[num_words], temp_wide_word, strlen(temp_word) + 1);
    num_words++;
  }
        
  /* Make sure list is terminated with null character */
  word_list[num_words][0] = '\0';

  DOUT(num_words);

//  if (num_words == 0)
//    UseAlphabet( );

  fclose(wordFile);

  /* Make list of all unicode characters used in word list: */
  /* (we use this to check to make sure all are "typable"); */
  gen_char_list();

  LOG("Leaving GenerateWordList()\n");

  return (num_words);
}






/* This version creates the letters using TTF_RenderUNICODE_Blended */
int RenderLetters(const TTF_Font* letter_font)
{
  Uint16 t[2];
  int i, j;  /* i is chars attempted, j is chars actually rendered. */

  if (!letter_font)
  {
    fprintf(stderr, "RenderLetters() - invalid TTF_Font* argument!\n");
    return 0;
  }

  i = j = num_chars_used = 0;

  t[1] = '\0';

  while (i < MAX_UNICODES)
  {
    t[0] = keyboard_list[i].unicode_value;

    if (t[0] != 0)
    {
      DEBUGCODE
      {
        fprintf(stderr, "Creating SDL_Surface for list element %d, char = '%lc', Unicode value = %d\n", i, *t, *t);
      }

      char_glyphs[j].unicode_value = t[0];
      char_glyphs[j].white_glyph = BlackOutline_Unicode(t, letter_font, &white);
      char_glyphs[j].red_glyph = BlackOutline_Unicode(t, letter_font, &red);

      j++;
      num_chars_used++;
    }
    i++;
  }

  return num_chars_used;
}



void FreeLetters(void)
{
  int i;

  for (i = 0; i < num_chars_used; i++)
  {
    SDL_FreeSurface(char_glyphs[i].white_glyph);
    SDL_FreeSurface(char_glyphs[i].red_glyph);
    char_glyphs[i].unicode_value = 0;
    char_glyphs[i].white_glyph = NULL;
    char_glyphs[i].red_glyph = NULL;
  } 
  /* List now empty: */
  num_chars_used = 0;
}


SDL_Surface* GetWhiteGlyph(wchar_t t)
{
  int i;

  for (i = 0;
       (char_glyphs[i].unicode_value != t) && (i <= num_chars_used);
       i++)
  {}

  /* Now return appropriate pointer: */
  if (i > num_chars_used)
  {
    /* Didn't find character: */
    fprintf(stderr, "Could not find glyph for Unicode char '%C', value = %d\n", t, t);
    return NULL;
  }
  
  /* Return corresponding surface for blitting: */
  return char_glyphs[i].white_glyph;
}



SDL_Surface* GetRedGlyph(wchar_t t)
{
  int i;

  for (i = 0;
       char_glyphs[i].unicode_value != t && i <= num_chars_used;
       i++)
  {}

  /* Now return appropriate pointer: */
  if (i > num_chars_used)
  {
    /* Didn't find character: */
    fprintf(stderr, "Could not find glyph for unicode character %lc\n", t);
    return NULL;
  }
  
  /* Return corresponding surface for blitting: */
  return char_glyphs[i].red_glyph;
}


/* Checks to see if all of the glyphs needed by the word list have been     */
/* successfully rendered based on the Unicode values given in keyboard.lst. */
/* If not, then the list contains characters that will not display and (if  */
/* keyboard.lst is correct) cannot be typed. Most likely, this means that   */
/* keyboard.lst is not correct.
/* Returns 1 if all needed chars found, 0 otherwise.                        */
int CheckNeededGlyphs(void)
{
  int i = 0;

  while ((i < MAX_UNICODES)
      && (char_list[i] != '\0'))
  {
    if (!GetWhiteGlyph(char_list[i]))
    {
      fprintf(stderr, "\nCheckNeededGlyphs() - needed char '%C' (Unicode value = %d) not found.\n",
              char_list[i], char_list[i]);
      fprintf(stderr, "This probably means that the theme's 'keyboard.lst' file is incorrect or incomplete.\n");
      return 0;
    }
    i++;
  }
  LOG("CheckNeededGlyphs() - all chars found.\n");
  return 1;
}

int check_needed_unicodes_str(const wchar_t* s)
{
  int i = 0;

  while ((i < MAX_WORD_SIZE)
      && (s[i] != '\0'))
  {
    if (!unicode_in_key_list(s[i]))
    {
      fprintf(stderr, "\ncheck_needed_unicodes_str() - needed char '%C' (Unicode value = %d) not found.\n",
              s[i], s[i]);
      return 0;
    }
    i++;
  }
  return 1;
}

/****************************************************/
/*                                                  */
/*       Local ("private") functions:               */
/*                                                  */
/****************************************************/


/* Creates a list of distinct Unicode characters in */
/* word_list[][] (so the program knows what         */
/* needs to be rendered for the games)              */
static void gen_char_list(void)
{
  int i, j;
  i = j = 0;
  char_list[0] = '\0';

  while (word_list[i][0] != '\0' && i < MAX_NUM_WORDS) 
  {
    j = 0;

    while (word_list[i][j]!= '\0' && j < MAX_WORD_SIZE)
    {
      add_char(word_list[i][j]);
      j++;
    }

    i++;
  }

  DEBUGCODE
  {
    fprintf(stderr, "char_list = %S\n", char_list);
  }
}



void ResetCharList(void)
{
  char_list[0] = '\0';
}



/* Creates a list of distinct Unicode characters in       */
/* the argument string for subsequent rendering.          */
/* Like gen_char_list() but takes a string argument       */
/* instead of going through the currently selected        */
/* word list. Argument should be UTF-8                    */
/* Can be called multiple times on different strings      */
/* to accumulate entire repertoire - call ResetCharList() */
/* to start over                                          */
void GenCharListFromString(const unsigned char* UTF8_str)
{
  int i = 0;
  wchar_t wchar_buf[MAX_UNICODES];

  ConvertFromUTF8(wchar_buf, UTF8_str);

  /* FNLEN is max length of phrase (I think) */
  while (wchar_buf[i] != '\0' && i < FNLEN) 
  {
    add_char(wchar_buf[i]);
    i++;
  }

  DEBUGCODE
  {
    fprintf(stderr, "char_list = %S\n", char_list);
  }
}



/* FIXME this function is currently dead code */
/* --- setup the alphabet --- */
static void set_letters(unsigned char *t) {
	int i;

	ALPHABET_SIZE = 0;
	for (i=0; i<256; i++)
		ALPHABET[i]=0;

	for (i=0; i<strlen(t); i++)
		if (t[i]!=' ') {
			ALPHABET[(int)t[i]]=1;
			ALPHABET_SIZE++;
		}
}



/* Checks to see if the argument is already in the list and adds    */
/* it if necessary.  Returns 1 if char added, 0 if already in list, */
/* -1 if list already up to maximum size:                           */
/* FIXME performance would be better with hashtable                 */
static int add_char(wchar_t uc)
{
  int i = 0;
  while ((char_list[i] != uc)
      && (char_list[i] != '\0')
      && (i < MAX_UNICODES - 1))          //Because 1 need for null terminator
  {
    i++;
  }

  /* unicode already in list: */
  if (char_list[i] == uc)
  {
    DEBUGCODE{ fprintf(stderr,
                       "Unicode value: %d\tcharacter %lc already in list\n",
                        uc, uc);}
    return 0;
  }

  if (char_list[i] == '\0')
  {
    DEBUGCODE{ fprintf(stderr, "Adding unicode value: %d\tcharacter %lc\n", uc, uc);}
    char_list[i] = uc;
    char_list[i + 1] = '\0';
    return 1;
  }

  if (i == MAX_UNICODES - 1)            //Because 1 need for null terminator
  {
    LOG ("Unable to add unicode - list at max capacity");
    return -1;
  }
}



static void clear_keyboard(void)
{
  int i = 0;
  for (i = 0; i < MAX_UNICODES; i++)
  {
    keyboard_list[i].unicode_value = 0;
    keyboard_list[i].finger = -1;
  }
}


/* This function just tidies up all the ptr args needed for      */
/* ConvertUTF8toUTF32() from Unicode, Inc. into a neat wrapper.  */
/* It returns -1 on error, otherwise returns the length of the   */
/* converted, null-terminated wchar_t* string now stored in the  */
/* location of the 'wide_word' pointer.                          */
int ConvertFromUTF8(wchar_t* wide_word, const unsigned char* UTF8_word)
{
  int i = 0;
  ConversionResult result;
  UTF8 temp_UTF8[FNLEN];
  UTF32 temp_UTF32[FNLEN];

  const UTF8* UTF8_Start = temp_UTF8;
  const UTF8* UTF8_End = &temp_UTF8[FNLEN-1];
  UTF32* UTF32_Start = temp_UTF32;
  UTF32* UTF32_End = &temp_UTF32[FNLEN-1];

  strncpy(temp_UTF8, UTF8_word, FNLEN);

  ConvertUTF8toUTF32(&UTF8_Start, UTF8_End,
                     &UTF32_Start, UTF32_End, 0);

  wide_word[0] = '\0';

  while ((i < FNLEN) && (temp_UTF32[i] != '\0'))
  {
    wide_word[i] = temp_UTF32[i];
    i++; 
  }

  if (i >= FNLEN)
  {
    fprintf(stderr, "convert_from_UTF8(): buffer overflow\n");
    return -1;
  }
  else  //need terminating null:
  {
    wide_word[i] = '\0';
  }

  DEBUGCODE {fprintf(stderr, "wide_word = %ls\n", wide_word);}

  return wcslen(wide_word);
}
