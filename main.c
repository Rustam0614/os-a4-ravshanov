/*
 * ILON ARENASI - C tilidagi konsol o'yini (slither.io ga o'xshash)
 * Operatsion tizimlar fani, 4-topshiriq (erkin mavzu)
 * Muallif: Ravshanov Rustamjon
 *
 * Qoidalar:
 *  - To'rtburchak arena, ichida ko'p ovqatchalar bor.
 *  - Ilon silliq, egiluvchan: bosh ko'rsatkich tomonga buriladi, tana uning izidan boradi.
 *  - Ovqatni yesangiz ilon o'sadi va OCHKO oshadi (tepada ko'rinadi).
 *    Yeyilgan ovqat yo'qoladi va boshqa joyda yangisi paydo bo'ladi.
 *  - Bot ilonlar yuradi. Siz botning tanasiga tegsangiz - o'yin tugaydi.
 *    Bot sizga tegsa - u o'ladi va ovqatga aylanadi.
 *  - O'z tanangizga tegish xavfsiz. Devorga tegsangiz o'yin tugaydi.
 *  - Boshqarish: sichqoncha/touchpad (yoki W A S D, strelkalar). Chiqish: Q.
 *
 * Chizish: har bir terminal belgisi 2x4 ta mayda nuqtadan iborat (Braille belgilari),
 * shuning uchun ilon katakchalardan emas, silliq chiziladi.
 */
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define PI 3.14159265358979323846

/* ---------- Sozlamalar (xohlasangiz o'zgartiring) ---------- */
#define MAX_AC 120               /* arenaning eng katta kengligi (terminal belgilarida) */
#define MAX_AR 40                /* arenaning eng katta balandligi (terminal qatorlarida) */
#define MAX_PW (MAX_AC * 2)      /* nuqtalardagi o'lcham: har belgi 2 nuqta eni, */
#define MAX_PH (MAX_AR * 4)      /* 4 nuqta bo'yi */
#define MAX_SEG 400              /* ilon bo'laklarining eng ko'pi */
#define MAX_OVQAT 900            /* arenadagi ovqatlarning eng ko'pi */
#define BOT_SONI 4               /* bot ilonlar soni */
#define ILON_SONI (BOT_SONI + 1) /* 0-raqam: o'yinchi, 1..BOT_SONI: botlar */

#define BOSH_UZUNLIK 12          /* o'yinchi ilonining boshlang'ich bo'lak soni */
#define BOT_BOSH_UZUNLIK 10
#define BOT_MAX_SEG 80           /* bot shundan uzunroq o'smaydi */
#define OSISH 3                  /* bitta ovqat necha bo'lak qo'shadi */
#define BO_LAK_ORALIG 1.8        /* qo'shni bo'laklar orasidagi masofa (nuqta) */
#define TANA_R 2.3               /* tana yo'g'onligi (radius, nuqta) */
#define BOSH_R 2.7
#define OVQAT_R 1.3
#define YEYISH_R 4.0             /* bosh ovqatga shu masofagacha yaqinlashsa yeydi */
#define TEGISH_R 3.8             /* ilonlar shu masofada tegishgan hisoblanadi */
#define TEZLIK 1.3               /* o'yinchi tezligi (nuqta/qadam) */
#define BOT_TEZLIK 1.15
#define BURILISH 0.16            /* bir qadamda eng ko'pi bilan burilish (radian) */
#define BOT_BURILISH 0.14
#define QADAM_MS 40              /* bir kadr (qadam) uchun millisekund */
#define BOT_TIRILISH 90          /* bot o'lgach necha qadamdan keyin qayta paydo bo'ladi */
#define DEVOR_MARJA 14.0         /* bot devorga shuncha yaqinlashsa, markazga buriladi */
#define XAVF_MASOFA 11.0         /* bot oldidagi ilonni shu masofadan payqaydi */
#define REKORD_FAYL "rekord.txt"

typedef struct {
    double x;
    double y;
    int rang; /* 0..3 */
} Ovqat;

typedef struct {
    double x[MAX_SEG]; /* x[0], y[0] - bosh; oxirgi bo'lak - dum */
    double y[MAX_SEG];
    int uzunlik;
    double burchak;    /* harakat yo'nalishi (radian) */
    int tirik;
    int kutish;        /* o'lgan bot qayta paydo bo'lishigacha qolgan qadamlar */
    int ehtiyot;       /* bot ehtiyotkormi (boshqa ilonlardan qochadimi) */
    int soat;          /* ehtiyotkorlik qachon qayta tanlanishini sanaydi */
} Ilon;

static Ilon ilonlar[ILON_SONI];
static Ovqat ovqat[MAX_OVQAT];
static int ovqat_soni;
static int oddiy_ovqat;                 /* arenada doim shuncha ovqat bo'lishga harakat qilinadi */
static int AC, AR, PW, PH;              /* arena o'lchami: belgilarda va nuqtalarda */
static unsigned char kanvas[MAX_PH][MAX_PW]; /* nuqtalar jadvali: 0 - bo'sh, boshqa - rang kodi */

static int ochko;                       /* yeyilgan ovqatlar soni */
static const char *sabab_matni = "";
static int boshqaruv_rejimi = 0;        /* 0: yo'q, 1: sichqoncha, 2: klaviatura */
static double sichqon_x, sichqon_y;     /* ko'rsatkich o'rni (arena nuqtalarida) */
static double klaviatura_burchak = 0.0;

static struct termios asl_sozlama;
static int sozlama_saqlangan = 0;
static volatile sig_atomic_t toxtatish = 0;

/* ================= Terminal ================= */

static void terminalni_tikla(void)
{
    printf("\033[?1003l\033[?1006l"); /* sichqoncha kuzatuvini o'chirish */
    printf("\033[?25h\033[0m\n");     /* kursorni qaytarish, ranglarni tozalash */
    fflush(stdout);
    if (sozlama_saqlangan) {
        tcsetattr(STDIN_FILENO, TCSANOW, &asl_sozlama);
    }
}

/*
 * Terminalni "xom" rejimga o'tkazadi (tugma bosilishi bilan darrov o'qiladi, ekranda ko'rinmaydi)
 * va sichqoncha harakatini kuzatishni yoqadi (xterm: 1003 va 1006 rejimlari).
 */
static void terminalni_sozla(void)
{
    struct termios yangi;

    if (tcgetattr(STDIN_FILENO, &asl_sozlama) == -1) {
        perror("Bu dastur terminalda ishlashi kerak");
        exit(1);
    }
    sozlama_saqlangan = 1;
    atexit(terminalni_tikla);

    yangi = asl_sozlama;
    yangi.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
    yangi.c_cc[VMIN] = 0;
    yangi.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &yangi);

    setvbuf(stdout, NULL, _IOFBF, 1 << 17); /* kadr bir yo'la ekranga chiqishi uchun katta bufer */
    printf("\033[?25l");
    printf("\033[?1003h\033[?1006h");
    fflush(stdout);
}

static void signal_ushla(int signo)
{
    (void)signo;
    toxtatish = 1;
}

static long hozir_ms(void)
{
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long)t.tv_sec * 1000L + t.tv_nsec / 1000000L;
}

static void dam_ol(long millisekund)
{
    struct timespec t;

    if (millisekund <= 0) {
        return;
    }
    t.tv_sec = millisekund / 1000;
    t.tv_nsec = (millisekund % 1000) * 1000000L;
    nanosleep(&t, NULL);
}

/*
 * Terminaldan kelgan hamma narsani o'qiydi:
 *  - sichqoncha: ESC [ < tugma ; ustun ; qator M    (SGR formati)
 *  - strelkalar: ESC [ A/B/C/D
 *  - oddiy tugmalar: w a s d q
 * Sichqoncha o'rnini arena nuqtalariga o'girib, global o'zgaruvchiga yozadi.
 * Qaytaradi: oxirgi bosilgan tugma yoki 0.
 */
static int kirishni_oqi(void)
{
    unsigned char buf[1024];
    int tugma = 0;
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf) - 1);

    if (n <= 0) {
        return 0;
    }
    buf[n] = '\0';

    for (ssize_t i = 0; i < n; i++) {
        if (buf[i] == 27) {
            if (i + 2 < n && buf[i + 1] == '[') {
                if (buf[i + 2] == '<') {
                    int b;
                    int ustun;
                    int qator;
                    int ishlatilgan = 0;
                    char tur;

                    if (sscanf((const char *)&buf[i + 3], "%d;%d;%d%c%n", &b, &ustun, &qator, &tur, &ishlatilgan) == 4) {
                        /* arena 2-ustun va 3-qatordan boshlanadi; har belgi 2x4 nuqta */
                        sichqon_x = (ustun - 2) * 2 + 1.0;
                        sichqon_y = (qator - 3) * 4 + 2.0;
                        boshqaruv_rejimi = 1;
                        i += 3 + ishlatilgan - 1;
                    }
                } else {
                    switch (buf[i + 2]) {
                    case 'A': tugma = 'w'; break;
                    case 'B': tugma = 's'; break;
                    case 'C': tugma = 'd'; break;
                    case 'D': tugma = 'a'; break;
                    default: break;
                    }
                    i += 2;
                }
            }
        } else {
            int k = tolower(buf[i]);
            if (k == 'w' || k == 'a' || k == 's' || k == 'd' || k == 'q') {
                tugma = k;
            }
        }
    }
    return tugma;
}

/* Ruxsat etilgan tugmalardan biri bosilguncha kutadi */
static int kutish(const char *ruxsat)
{
    char c;

    for (;;) {
        if (toxtatish) {
            return 'q';
        }
        if (read(STDIN_FILENO, &c, 1) == 1) {
            int k = tolower((unsigned char)c);
            if (strchr(ruxsat, k) != NULL) {
                return k;
            }
        }
        dam_ol(30);
    }
}

/* ================= Rekord (fayl) ================= */

static int rekord_oqi(void)
{
    int rekord = 0;
    FILE *fp = fopen(REKORD_FAYL, "r");

    if (fp == NULL) {
        return 0; /* birinchi marta ishga tushganda fayl yo'q - bu xato emas */
    }
    if (fscanf(fp, "%d", &rekord) != 1) {
        rekord = 0;
    }
    fclose(fp);
    return rekord;
}

static void rekord_yoz(int rekord)
{
    FILE *fp = fopen(REKORD_FAYL, "w");

    if (fp == NULL) {
        perror(REKORD_FAYL);
        return;
    }
    fprintf(fp, "%d\n", rekord);
    fclose(fp);
}

/* ================= Geometriya va ilon harakati ================= */

static double tasodifiy(double dan, double gacha)
{
    return dan + (gacha - dan) * (rand() / (double)RAND_MAX);
}

/* Burchakni -PI..PI oralig'iga keltiradi */
static double burchak_tuzat(double a)
{
    while (a > PI) {
        a -= 2 * PI;
    }
    while (a < -PI) {
        a += 2 * PI;
    }
    return a;
}

/* Ilonning boshini maqsad burchagi tomon burchakli tezlik chegarasida buradi */
static void burilish(Ilon *il, double maqsad, double chegara)
{
    double farq = burchak_tuzat(maqsad - il->burchak);

    if (farq > chegara) {
        farq = chegara;
    } else if (farq < -chegara) {
        farq = -chegara;
    }
    il->burchak = burchak_tuzat(il->burchak + farq);
}

/*
 * Ilonni oldinga yurgizadi: bosh yo'nalish bo'yicha siljiydi,
 * har bir keyingi bo'lak oldingisini "quvadi" va undan BO_LAK_ORALIG masofada turadi.
 * Shu tufayli ilon silliq egiladi.
 */
static void oldinga(Ilon *il, double tezlik)
{
    il->x[0] += cos(il->burchak) * tezlik;
    il->y[0] += sin(il->burchak) * tezlik;

    for (int i = 1; i < il->uzunlik; i++) {
        double dx = il->x[i - 1] - il->x[i];
        double dy = il->y[i - 1] - il->y[i];
        double d = sqrt(dx * dx + dy * dy);

        if (d > BO_LAK_ORALIG) {
            double k = (d - BO_LAK_ORALIG) / d;
            il->x[i] += dx * k;
            il->y[i] += dy * k;
        }
    }
}

/* Ilon k ta bo'lakka o'sadi (yangi bo'laklar dumda turadi va keyin ochilib chiqadi) */
static void osish(Ilon *il, int k, int chegara)
{
    for (int j = 0; j < k && il->uzunlik < chegara && il->uzunlik < MAX_SEG; j++) {
        il->x[il->uzunlik] = il->x[il->uzunlik - 1];
        il->y[il->uzunlik] = il->y[il->uzunlik - 1];
        il->uzunlik++;
    }
}

/* (x, y) nuqta ilonning boshlanish-dan boshlab biror bo'lagiga tegyaptimi? */
static int tegdimi(double x, double y, const Ilon *il, int boshlanish)
{
    for (int i = boshlanish; i < il->uzunlik; i++) {
        double dx = il->x[i] - x;
        double dy = il->y[i] - y;
        if (dx * dx + dy * dy < TEGISH_R * TEGISH_R) {
            return 1;
        }
    }
    return 0;
}

static int devorga_tegdi(const Ilon *il)
{
    return il->x[0] < 1.5 || il->x[0] > PW - 1.5 || il->y[0] < 1.5 || il->y[0] > PH - 1.5;
}

/* ================= Ovqat ================= */

static void ovqat_ber(double x, double y)
{
    if (ovqat_soni < MAX_OVQAT) {
        ovqat[ovqat_soni].x = x;
        ovqat[ovqat_soni].y = y;
        ovqat[ovqat_soni].rang = rand() % 4;
        ovqat_soni++;
    }
}

/* Arenaning tasodifiy joyida ovqat paydo bo'ladi */
static void ovqat_tasodifiy(void)
{
    ovqat_ber(tasodifiy(4, PW - 4), tasodifiy(4, PH - 4));
}

/* Ovqat soni oddiy miqdordan kam bo'lsa, to'ldiradi */
static void ovqat_toldir(void)
{
    while (ovqat_soni < oddiy_ovqat) {
        ovqat_tasodifiy();
    }
}

/*
 * Ilon boshi yetgan barcha ovqatlarni yeydi (ovqat yo'qoladi), ilon o'sadi.
 * Qaytaradi: yeyilgan ovqatlar soni.
 */
static int ovqat_yeyish(Ilon *il, int uzunlik_chegarasi)
{
    int yeyilgan = 0;

    for (int i = 0; i < ovqat_soni;) {
        double dx = ovqat[i].x - il->x[0];
        double dy = ovqat[i].y - il->y[0];
        if (dx * dx + dy * dy < YEYISH_R * YEYISH_R) {
            ovqat[i] = ovqat[--ovqat_soni]; /* oxirgisini shu joyga ko'chiramiz */
            osish(il, OSISH, uzunlik_chegarasi);
            yeyilgan++;
        } else {
            i++;
        }
    }
    return yeyilgan;
}

/* ================= Botlar ================= */

/* Bot o'ladi: tanasi ovqatga aylanadi */
static void ilon_tugat(int id)
{
    Ilon *il = &ilonlar[id];

    for (int i = 0; i < il->uzunlik; i += 2) {
        ovqat_ber(il->x[i], il->y[i]);
    }
    il->tirik = 0;
    il->kutish = BOT_TIRILISH;
}

/* Botni arenadagi o'yinchidan uzoq joyga tug'diradi */
static void tugilish(int id)
{
    Ilon *il = &ilonlar[id];

    for (int urinish = 0; urinish < 100; urinish++) {
        double x = tasodifiy(20, PW - 20.0);
        double y = tasodifiy(20, PH - 20.0);
        double b = tasodifiy(-PI, PI);

        if (ilonlar[0].tirik) {
            double dx = ilonlar[0].x[0] - x;
            double dy = ilonlar[0].y[0] - y;
            if (dx * dx + dy * dy < 50.0 * 50.0) {
                continue;
            }
        }
        il->uzunlik = BOT_BOSH_UZUNLIK;
        il->burchak = b;
        for (int i = 0; i < il->uzunlik; i++) {
            il->x[i] = x - cos(b) * BO_LAK_ORALIG * i;
            il->y[i] = y - sin(b) * BO_LAK_ORALIG * i;
        }
        il->tirik = 1;
        il->soat = 0;
        return;
    }
    il->kutish = 5; /* joy topilmadi - biroz keyin yana urinamiz */
}

/*
 * Bot qaysi tomonga burilishi kerakligini hal qiladi:
 *  1) eng yaqin ovqat tomon;
 *  2) devorga yaqinlashsa - arena markaziga;
 *  3) ehtiyotkor bo'lsa va oldida boshqa ilon bo'lsa - undan chetga.
 */
static double bot_maqsad(int id)
{
    Ilon *il = &ilonlar[id];
    double hx = il->x[0];
    double hy = il->y[0];
    double b = il->burchak;
    double maqsad = b;
    double eng = 1e18;

    for (int i = 0; i < ovqat_soni; i++) {
        double dx = ovqat[i].x - hx;
        double dy = ovqat[i].y - hy;
        double d2 = dx * dx + dy * dy;
        if (d2 < eng) {
            eng = d2;
            maqsad = atan2(dy, dx);
        }
    }
    maqsad += tasodifiy(-0.12, 0.12); /* sal tebranish - bot tabiiyroq yursin */

    if (hx < DEVOR_MARJA || hx > PW - DEVOR_MARJA || hy < DEVOR_MARJA || hy > PH - DEVOR_MARJA) {
        maqsad = atan2(PH / 2.0 - hy, PW / 2.0 - hx);
    }

    if (il->ehtiyot) {
        double eng_yaqin = XAVF_MASOFA * XAVF_MASOFA;
        double cx = 0;
        double cy = 0;
        int topildi = 0;
        double ox = cos(b);
        double oy = sin(b);

        for (int j = 0; j < ILON_SONI; j++) {
            if (j == id || !ilonlar[j].tirik) {
                continue;
            }
            for (int k = 0; k < ilonlar[j].uzunlik; k++) {
                double dx = ilonlar[j].x[k] - hx;
                double dy = ilonlar[j].y[k] - hy;
                double d2 = dx * dx + dy * dy;
                if (d2 < eng_yaqin && dx * ox + dy * oy > 0) { /* oldimizda */
                    eng_yaqin = d2;
                    cx = dx;
                    cy = dy;
                    topildi = 1;
                }
            }
        }
        if (topildi) {
            double kesishma = ox * cy - oy * cx; /* to'siq qaysi tomonda: chap yoki o'ng */
            maqsad = b + (kesishma > 0 ? -1.3 : 1.3);
        }
    }
    return maqsad;
}

static void botlar_yurgiz(void)
{
    for (int id = 1; id < ILON_SONI; id++) {
        Ilon *il = &ilonlar[id];

        if (!il->tirik) {
            if (--il->kutish <= 0) {
                tugilish(id);
            }
            continue;
        }
        if (il->soat-- <= 0) { /* ba'zan bot "e'tiborsiz" bo'lib qoladi */
            il->ehtiyot = rand() % 100 < 70;
            il->soat = 40 + rand() % 60;
        }

        burilish(il, bot_maqsad(id), BOT_BURILISH);
        oldinga(il, BOT_TEZLIK);

        if (devorga_tegdi(il)) {
            ilon_tugat(id);
            continue;
        }
        /* boshqa ilonga (o'yinchiga ham) tegsa - bot o'ladi va ovqatga aylanadi */
        int halok = 0;
        for (int j = 0; j < ILON_SONI && !halok; j++) {
            if (j != id && ilonlar[j].tirik && tegdimi(il->x[0], il->y[0], &ilonlar[j], 0)) {
                halok = 1;
            }
        }
        if (halok) {
            ilon_tugat(id);
            continue;
        }
        ovqat_yeyish(il, BOT_MAX_SEG);
    }
}

/* ================= Chizish ================= */

/* Kanvasda (cx, cy) markazli, r radiusli doira chizadi. Katta kod ustun turadi. */
static void doira(double cx, double cy, double r, unsigned char kod)
{
    int x0 = (int)floor(cx - r);
    int x1 = (int)ceil(cx + r);
    int y0 = (int)floor(cy - r);
    int y1 = (int)ceil(cy + r);

    for (int y = y0; y <= y1; y++) {
        if (y < 0 || y >= PH) {
            continue;
        }
        for (int x = x0; x <= x1; x++) {
            double dx = x + 0.5 - cx;
            double dy = y + 0.5 - cy;
            if (x >= 0 && x < PW && dx * dx + dy * dy <= r * r && kanvas[y][x] < kod) {
                kanvas[y][x] = kod;
            }
        }
    }
}

/* Rang kodidan 256 rangli palitradagi rang raqamini topadi */
static int rang_raqami(int kod)
{
    static const int OVQAT_RANG[4] = {203, 227, 207, 87};
    static const int TANA_RANG[ILON_SONI] = {34, 37, 172, 127, 27};
    static const int BOSH_RANG[ILON_SONI] = {82, 51, 226, 213, 75};

    if (kod >= 20) {
        return BOSH_RANG[(kod - 20) % ILON_SONI];
    }
    if (kod >= 10) {
        return TANA_RANG[(kod - 10) % ILON_SONI];
    }
    return OVQAT_RANG[(kod - 1) % 4];
}

/* Bitta Braille belgisini (U+2800..U+28FF) UTF-8 ko'rinishida chiqaradi */
static void braille_chiqar(int maska)
{
    putchar(0xE2);
    putchar(0xA0 + (maska >> 6));
    putchar(0x80 + (maska & 0x3F));
}

static void chiz(int rekord)
{
    /* Braille nuqtalarining bitlari: [qator][ustun] */
    static const int BIT[4][2] = {{0x01, 0x08}, {0x02, 0x10}, {0x04, 0x20}, {0x40, 0x80}};
    int tirik_botlar = 0;

    for (int y = 0; y < PH; y++) {
        memset(kanvas[y], 0, (size_t)PW);
    }
    for (int i = 0; i < ovqat_soni; i++) {
        doira(ovqat[i].x, ovqat[i].y, OVQAT_R, (unsigned char)(1 + ovqat[i].rang));
    }
    for (int id = 0; id < ILON_SONI; id++) {
        const Ilon *il = &ilonlar[id];
        if (!il->tirik) {
            continue;
        }
        if (id > 0) {
            tirik_botlar++;
        }
        for (int i = il->uzunlik - 1; i >= 0; i--) {
            if (i == 0) {
                doira(il->x[i], il->y[i], BOSH_R, (unsigned char)(20 + id));
            } else {
                doira(il->x[i], il->y[i], TANA_R, (unsigned char)(10 + id));
            }
        }
    }

    printf("\033[H");
    printf("\033[1m  OCHKO: %d\033[0m  (yeyilgan ovqatlar)   Rekord: %d   Uzunlik: %d   Botlar: %d\033[K\n",
           ochko, rekord, ilonlar[0].tirik ? ilonlar[0].uzunlik : 0, tirik_botlar);

    printf("\033[100m");
    for (int x = 0; x < AC + 2; x++) {
        putchar(' ');
    }
    printf("\033[0m\n");

    for (int cy = 0; cy < AR; cy++) {
        int joriy_rang = -1;

        printf("\033[100m \033[0m");
        for (int cx = 0; cx < AC; cx++) {
            int maska = 0;
            int kod = 0;

            for (int dy = 0; dy < 4; dy++) {
                for (int dx = 0; dx < 2; dx++) {
                    int k = kanvas[cy * 4 + dy][cx * 2 + dx];
                    if (k != 0) {
                        maska |= BIT[dy][dx];
                        if (k > kod) {
                            kod = k;
                        }
                    }
                }
            }
            if (maska == 0) {
                putchar(' ');
            } else {
                int rang = rang_raqami(kod);
                if (rang != joriy_rang) {
                    printf("\033[38;5;%dm", rang);
                    joriy_rang = rang;
                }
                braille_chiqar(maska);
            }
        }
        printf("\033[0m\033[100m \033[0m\n");
    }

    printf("\033[100m");
    for (int x = 0; x < AC + 2; x++) {
        putchar(' ');
    }
    printf("\033[0m\n");
    printf("  Sichqoncha/touchpad: ilon ko'rsatkich tomonga yuradi | W A S D | Q - chiqish\033[K");
    fflush(stdout);
}

/* ================= O'yin ================= */

/* Arena o'lchamini terminal o'lchamiga moslaydi. Terminal juda kichik bo'lsa -1 */
static int maydon_sozla(int ustun, int qator)
{
    AC = ustun - 2;
    if (AC > MAX_AC) {
        AC = MAX_AC;
    }
    AR = qator - 8; /* sarlavha, 2 ta devor, pastki yozuv va o'yin tugagach xabarlar uchun joy */
    if (AR > MAX_AR) {
        AR = MAX_AR;
    }
    if (AC < 40 || AR < 12) {
        return -1;
    }
    PW = AC * 2;
    PH = AR * 4;
    oddiy_ovqat = PW * PH / 350;
    if (oddiy_ovqat < 25) {
        oddiy_ovqat = 25;
    }
    return 0;
}

/* Yangi o'yin boshlanishi: ilonlar, ovqatlar joylanadi */
static void oyinni_boshla(void)
{
    Ilon *o = &ilonlar[0];

    ovqat_soni = 0;
    ochko = 0;
    boshqaruv_rejimi = 0;
    for (int id = 0; id < ILON_SONI; id++) {
        ilonlar[id].tirik = 0;
        ilonlar[id].kutish = 0;
    }

    o->uzunlik = BOSH_UZUNLIK;
    o->burchak = 0.0;
    o->tirik = 1;
    for (int i = 0; i < o->uzunlik; i++) {
        o->x[i] = PW / 2.0 - BO_LAK_ORALIG * i;
        o->y[i] = PH / 2.0;
    }
    for (int id = 1; id < ILON_SONI; id++) {
        tugilish(id);
    }
    ovqat_toldir();
}

/*
 * Bitta qadam: o'yinchini va botlarni yurgizadi, to'qnashuvlarni tekshiradi.
 * tugma - bosilgan tugma (yoki 0). Qaytaradi: 1 - o'yinchi halok bo'ldi, 0 - o'yin davom etadi.
 */
static int qadam(int tugma)
{
    Ilon *o = &ilonlar[0];

    if (tugma != 0) { /* klaviatura: ilon shu tomonga buriladi */
        klaviatura_burchak = (tugma == 'w') ? -PI / 2 : (tugma == 's') ? PI / 2 : (tugma == 'a') ? PI : 0.0;
        boshqaruv_rejimi = 2;
    }
    if (boshqaruv_rejimi == 2) {
        burilish(o, klaviatura_burchak, BURILISH);
    } else if (boshqaruv_rejimi == 1) {
        double dx = sichqon_x - o->x[0];
        double dy = sichqon_y - o->y[0];
        if (dx * dx + dy * dy > 16.0) { /* ko'rsatkichga juda yaqin bo'lsa, to'g'ri yuradi */
            burilish(o, atan2(dy, dx), BURILISH);
        }
    }
    oldinga(o, TEZLIK);

    if (devorga_tegdi(o)) {
        sabab_matni = "devorga urildingiz";
        return 1;
    }
    /* bot ilonning tanasiga tegsa halok bo'lasiz (bot boshiga tegish xavfsiz: bot o'ladi) */
    for (int id = 1; id < ILON_SONI; id++) {
        if (ilonlar[id].tirik && tegdimi(o->x[0], o->y[0], &ilonlar[id], 1)) {
            sabab_matni = "bot ilonga urildingiz";
            return 1;
        }
    }
    /* o'z tanangizga tegish xavfsiz - tekshirilmaydi */

    ochko += ovqat_yeyish(o, MAX_SEG);
    botlar_yurgiz();
    ovqat_toldir();
    return 0;
}

/*
 * Bitta o'yin. Qaytaradi: ochko; o'yinchi Q bossa -1; terminal juda kichik bo'lsa -2.
 */
static int bir_oyin(int rekord)
{
    struct winsize ws;
    int ustun = 80;
    int qator = 24;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        ustun = ws.ws_col;
        qator = ws.ws_row;
    }
    if (maydon_sozla(ustun, qator) != 0) {
        sabab_matni = "terminal oynasi juda kichik (kamida 42x20 kerak)";
        return -2;
    }
    oyinni_boshla();
    printf("\033[2J");

    for (;;) {
        long boshlandi = hozir_ms();

        chiz(rekord);
        int t = kirishni_oqi();
        if (t == 'q' || toxtatish) {
            return -1;
        }
        if (qadam(t)) {
            break;
        }
        dam_ol(QADAM_MS - (hozir_ms() - boshlandi)); /* kadr tezligi bir xil bo'lsin */
    }

    chiz(rekord);
    return ochko;
}

int main(void)
{
    srand((unsigned int)time(NULL));
    signal(SIGINT, signal_ushla);
    terminalni_sozla();

    int rekord = rekord_oqi();

    printf("\033[2J\033[H");
    printf("==============================\n");
    printf("        I L O N  ARENASI\n");
    printf("==============================\n\n");
    printf("Ovqatchalarni yeng - ilon o'sadi, OCHKO oshadi.\n");
    printf("Bot ilonning tanasiga tegsangiz - o'yin tugaydi.\n");
    printf("Bot sizga tegsa - u o'ladi va ovqatga aylanadi.\n");
    printf("O'z tanangizga tegish xavfsiz. Devorga tegmang!\n\n");
    printf("Boshqarish: sichqoncha yoki touchpad (ilon ko'rsatkich tomonga yuradi)\n");
    printf("Zaxira: W A S D yoki strelkalar. Chiqish: Q\n");
    printf("Terminal oynasini katta qiling (arena oyna o'lchamiga moslanadi).\n\n");
    printf("Boshlash uchun ENTER yoki probel bosing...\n");
    fflush(stdout);
    if (kutish("\n \r") == 'q') {
        return 0;
    }

    for (;;) {
        int natija = bir_oyin(rekord);

        if (natija == -1) {
            break;
        }
        if (natija == -2) {
            printf("\033[2J\033[H%s.\nOynani kattalashtirib, qayta ishga tushiring.\n", sabab_matni);
            break;
        }

        printf("\n  O'YIN TUGADI: %s.  OCHKO: %d\n", sabab_matni, natija);
        if (natija > rekord) {
            rekord = natija;
            rekord_yoz(rekord);
            printf("  YANGI REKORD!\n");
        }
        printf("  Qayta o'ynash: R   |   Chiqish: Q\n");
        fflush(stdout);

        dam_ol(600);
        kirishni_oqi(); /* o'yin oxirida bosilgan tugmalarni tashlab yuboramiz */
        if (kutish("rq") == 'q') {
            break;
        }
    }

    printf("\nXayr! Rekord: %d\n", rekord);
    return 0;
}
