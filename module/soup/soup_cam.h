/* Playback and develop defaults for the EOS 7D (firmware 2.0.3). */
#ifndef SOUP_CAM_H
#define SOUP_CAM_H

/* Canon Small JPEG on the 7D: half of the 5184x3456 sensor, so the
 * half-scale develop of the full-size CR2 preview lands on it exactly. */
#define FILM_PLAY_W         2592
#define FILM_PLAY_H         1728
#define FILM_MIN_CR2        (8u * 1024 * 1024)
#define FILM_MODEL_FALLBACK "Canon EOS 7D"

#endif
