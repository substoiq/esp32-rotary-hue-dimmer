/* Alle keuzes van de app op één plek. */
#pragma once

/* Hue */
#define HUE_ZONE_NAME        "StudeerZone"  /* exacte naam in de Hue-app */
#define UI_ROOM_LABEL        "studeerzone"  /* tekst op het scherm */

/* Dimmen */
#define DIM_ON_PERCENT       30   /* de knop zet de lampen altijd hierop aan */
#define DIM_MIN_PERCENT      1
#define DIM_MAX_PERCENT      100
#define DIM_STEP_PERCENT     2    /* per klik van de ring */

/* Verkeer naar de bridge */
#define HUE_SEND_INTERVAL_MS 500  /* minimale tijd tussen twee opdrachten */
#define HUE_TRANSITION_MS    400  /* vloeiende overgang van de lampen */
#define HUE_ECHO_IGNORE_MS   1500 /* negeer events zo lang na eigen bediening */

/* Hardware: alleen aanpassen als de test op het bord dat nodig maakt */
#define ENCODER_COUNTS_PER_STEP 4 /* tellingen per voelbare klik */
#define ENCODER_REVERSE      0    /* op 1 zetten als de draairichting omgekeerd is */
#define BACKLIGHT_PERCENT    60
