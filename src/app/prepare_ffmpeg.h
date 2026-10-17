#ifndef MOON_PREPARE_FFMPEG_H
#define MOON_PREPARE_FFMPEG_H

/* The first-run preparation step for the movie decoder. It runs in the launcher,
   before the game proper starts, on every launch path (the front-end and the game
   CLI). The menu's cutscene and its car are MPEG-PS movies decoded by spawning an
   external ffmpeg (src/platform/sdl/movie.c); a fresh machine that got only the
   executable has no decoder, so this makes sure one is found:
     1. already in the user's folder  -> nothing to do, no window;
     2. beside the executable         -> adopted into the user's folder;
     3. not found                     -> downloaded (BtbN FFmpeg-Builds, latest
        win64-gpl) in a small window that shows the progress, then verified and
        placed in the user's folder.
   A failure is said plainly, never blocks the game (the cutscenes are simply
   absent, as they are today), and is remembered for a day so it does not nag on
   every start. Off Windows this is a no-op: the decoder is only spawned there.

   `shot` (may be NULL): if set, one PNG of the preparation window is written
   while it is up, the same way --launcher-shot captures the front door. Set by
   the MOON_PREPARE_SHOT environment variable, for a machine nobody can sit at. */
void launcher_prepare_ffmpeg(void);

#endif
