# Ilon arenasi — C tilidagi konsol o'yini (slither.io'ga o'xshash)

- **Fan:** Operatsion tizimlar
- **Topshiriq:** 4-topshiriq (erkin mavzu)
- **Talaba:** Ravshanov Rustamjon
- **Guruh:** SRM002-2
- **O'qituvchi:** Durdona Bekimmetova
- **Mavzu:** Ilon arenasi o'yini (silliq egiluvchan ilon, sichqoncha/touchpad bilan boshqariladi, bot ilonlar)

## O'yin qoidalari

- To'rtburchak arena, ichida ko'p ovqatchalar bor.
- Ilon silliq egiladi: bosh ko'rsatkich tomonga buriladi, tana uning izidan boradi.
- Ovqatni yesangiz, ilon o'sadi va **OCHKO** oshadi (tepada ko'rinib turadi). Yeyilgan ovqat yo'qoladi va boshqa joyda yangisi paydo bo'ladi.
- Arenada 4 ta bot ilon yuradi. Botning **tanasiga** tegsangiz, o'yin tugaydi.
- Bot sizga tegsa, u o'ladi va butun tanasi ovqatga aylanadi. O'lgan bot birozdan keyin qaytadan paydo bo'ladi.
- O'z tanangizga tegish **xavfsiz**. Devorga tegsangiz o'yin tugaydi.
- Eng yaxshi hisob `rekord.txt` faylida saqlanadi.

## Boshqarish

- **Sichqoncha yoki touchpad:** ilon ko'rsatkich turgan tomonga yuradi (ko'rsatkich terminal oynasi ustida bo'lsin).
- **Zaxira:** `W A S D` yoki strelka tugmalari (ilon shu tomonga buriladi).
- **Chiqish:** `Q` yoki `Ctrl+C`.

## Kompilyatsiya va ishga tushirish

```bash
make            # gcc -Wall -Wextra -std=c11 -O2 main.c -o ilon -lm
./ilon          # yoki: make run
make clean      # tozalash
```

Terminal oynasi kamida 42x20 bo'lsin. Arena oyna o'lchamiga moslanadi, shuning uchun oynani katta qilish tavsiya etiladi.

## Dastur qanday ishlaydi

**Silliq harakat.** Ilon katakchalarda emas, o'lchami kasr bo'lgan nuqtalarda harakatlanadi:
- Ilon boshi `burchak` yo'nalishi bo'yicha oldinga siljiydi (`cos`, `sin`).
- Bosh maqsad burchak tomon har qadamda faqat chegaralangan miqdorda buriladi (`burilish()`), shuning uchun burilishlar silliq.
- Tananing har bir bo'lagi oldingi bo'lakni "quvadi" va undan bir xil masofada turadi (`oldinga()`), shuning uchun ilon egiladi.

**Chizish.** Har bir terminal belgisi 2x4 ta mayda nuqtadan iborat (Braille belgilari U+2800..U+28FF). Ilon, ovqat va botlar avval nuqtalar jadvaliga (`kanvas`) doira ko'rinishida chiziladi, keyin har 2x4 nuqta bitta belgiga aylantirilib rang bilan ekranga chiqariladi (`chiz()`).

**To'qnashuvlar** bo'laklar orasidagi masofa bo'yicha tekshiriladi (`tegdimi()`).

**Botlar** eng yaqin ovqat tomon yuradi, devordan qochadi, ehtiyotkor bo'lsa boshqa ilonlardan chetlanadi (`bot_maqsad()`). Ba'zan "e'tiborsiz" bo'ladi, shuning uchun urilib o'ladi.

| Qism | Vazifasi |
|---|---|
| `terminalni_sozla()` / `terminalni_tikla()` | Terminalni "xom" rejimga o'tkazadi (`termios`) va sichqoncha kuzatuvini yoqadi; chiqishda hammasini tiklaydi |
| `kirishni_oqi()` | Klaviatura va sichqoncha hodisalarini o'qiydi |
| `burilish()`, `oldinga()`, `osish()` | Ilon harakati: burilish, yurish, o'sish |
| `ovqat_yeyish()`, `ovqat_toldir()` | Ovqat yeyish va yangisini paydo qilish |
| `bot_maqsad()`, `botlar_yurgiz()` | Bot ilonlar mantiqi |
| `chiz()` | Arenani Braille belgilari va ranglar bilan chizadi |
| `qadam()` | Bitta o'yin qadami: yurish, to'qnashuv, ovqat, botlar |
| `rekord_oqi()` / `rekord_yoz()` | Rekordni fayldan o'qish va yozish |

## Ishlatilgan imkoniyatlar

Strukturalar, ikki o'lchamli massivlar, `<math.h>` (`sin`, `cos`, `atan2`, `sqrt`), `termios`,
`signal`, `ioctl` (terminal o'lchami), `clock_gettime`/`nanosleep` (kadr tezligi),
fayl bilan ishlash, ANSI escape kodlari.

## Fayllar

- `main.c` — dastur kodi
- `Makefile` — kompilyatsiya
- `README.md` — shu hujjat
