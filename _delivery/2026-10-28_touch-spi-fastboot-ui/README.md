# NovaLK — دعم اللمس عبر SPI + واجهة Fastboot جديدة

المشروع: `little-kernel_realme-c21` (Realme C21 / RMX3201 — MT6765)
التاريخ: 2026-10-28
الحالة: تعديلات غير مُودَعة في git (لا commit ولا push).

---

## 1. سبب فشل اللمس (من السجل)

سجل LK أظهر:

```
[I2C-LK] 367: id=0..6, addr: 48, transfer error
867: Write 5 bytes fails,ret=-121
```

أي أن كل نواقل I2C0..I2C6 جرّبت العنوان `0x48` ولم يستجب أي جهاز.

بينما سجل الكيرنل الرسمي يقول إن الشريحة تعمل:

```
himax_ic_package_check:Read driver IC ID = 83, 10, 2D
Himax IC package 83102d_in
```

وشجرة الجهاز الرسمية (`oppo6765_20701.dts`) تضع عقدة اللمس كابن لـ `&spi2`:

```
touch: touchscreen@0 {
    compatible = "oppo,tp_noflash";
    reg = <0>;
    spi-max-frequency = <15000000>;
    reset-gpio = <&pio 178 0>;
    irq-gpio   = <&pio 0 0x2002>;
};
```

**الاستنتاج:** متحكّم اللمس HX83102D موصول على **SPI2 (0x11012000)** وليس I2C.
كل محاولات I2C السابقة كانت تصل إلى لا شيء، وهذا هو سبب فشل اللمس.

المسارات (pins) حسب `ctp_ts_spi_mode` في نفس الـ DTS:

| GPIO | الوظيفة (function 2) |
|------|----------------------|
| 17   | SPI2_MO  |
| 18   | SPI2_CLK |
| 19   | SPI2_MI  |
| 20   | SPI2_CSB |

---

## 2. ما تم تنفيذه

### 2.1 ناقل SPI في LK
* `platform/mt6765` لا يحتوي على أي سائق SPI، و`platform/common/spi/spi.c` كان موجوداً
  لكنه غير مربوط بالبناء ولا يوجد له `rules.mk`.
* تمت إضافة `init_spi_bus_by_reg()` لتسجيل المتحكّم بعنوان التسجيل مباشرة
  (لأن شجرة LK المدمجة لا تحتوي على عقدة فرعية للمس).
* تم تفعيل الوحدة عبر `MTK_SPI_SUPPORT = yes` في `project/rmx2180.mk`
  (مُقيَّدة بهذا المتغيّر حتى لا تتعارض مع منصّات لها سائقها الخاص مثل mt6768).
* تم تقصير `MTK_TXRX_TIMEOUT_US` من 10 ثوانٍ إلى 500 مللي ثانية حتى لا يتجمّد
  الإقلاع إذا كان ساعة SPI2 مغلقة (clock gated) بدل انتظار طويل.

### 2.2 سائق اللمس على SPI — `tpd_himax_spi.c`
منقول حرفياً من سائق Himax الرسمي:

* الكتابة: إطار واحد `{0xF2, reg, data...}`
* القراءة: `{0xF3, reg, 0x00}` ثم N بايت، والحمولة تبدأ من البايت الرابع
* SPI mode 3 (`CPHA | CPOL`)، 8 بت، 15 MHz، CS0
* قراءة نافذة AHB:
  1. `burst_enable(len > 4)` → `0x13 ← 0x31`، `0x0D ← 0x10|auto`
  2. `0x00 ← عنوان 32-بت (little endian)`
  3. `0x0C ← 0x00`
  4. قراءة `0x08` بطول N
  5. `burst_enable(0)` إذا كان الطول > 4
* معرّف الشريحة: 4 بايت من `0x900000D0` ⇒ `0x83102D`
* قراءة الأحداث: `0x11 ← 0x00` ثم قراءة `0x30` بطول 56 بايت
* تحليل الإطار مثل السائق الأصلي:
  * `buf[52]` = عدد النقاط (`0xff` = لا شيء، وإلا `& 0x0f`)
  * النقطة i عند `buf[i*4 .. i*4+3]`
  * مجموع الـ 56 بايت & 0xff يجب أن يساوي 0 (checksum)
  * الإطار الصفري غير صالح
* إعادة الضبط (GPIO178): محاولة قراءة المعرّف أولاً بدون reset، وإذا فشلت:
  نبضة reset (low 5ms ثم high 50ms) ثم محاولة كل 25ms حتى 500ms.
* إذا فشل مسار DMA (الإطار 59 بايت > عمق FIFO 32) يتحول السائق تلقائياً
  إلى إطارين متتاليين مع إبقاء CS منخفضاً (pause/resume).

### 2.3 ترتيب التحسّس
`tpd_init()` أصبح: **SPI2 أولاً**، ثم مسار شجرة الجهاز، ثم مسح I2C (احتياطي فقط).

### 2.4 واجهة Fastboot الجديدة — `fastboot_ui.c`
نفس هوية قائمة الإقلاع بالضبط (نفس الألوان، نفس المقاييس، نفس المسافات):

```
              Nova LK
            FASTBOOT MODE
        REALME C21  -  RMX3201
               ────

     ┌────────────────────────┐
     │        FASTBOOT        │
     │ USB / bootloader iface │
     └────────────────────────┘

     ┌────────────────────────┐
     │ ●  WAITING FOR USB     │   ← تتحول إلى USB CONNECTED
     │    plug the cable ...  │      عند وصل الكابل
     └────────────────────────┘
     ──────────────────────────
          fastboot devices
       POWER + VOL-  ->  reboot
```

* تُرسم من `target_fastboot_init()` أي في كل طرق الدخول إلى fastboot.
* حالة USB تتحدّث من `fastboot_notify()` (UDC online/offline) بإعادة رسم
  بطاقة الحالة فقط.
* يتم استدعاء `mt65xx_backlight_on()` قبل الرسم لضمان ظهور الشاشة.

---

## 3. الملفات

### جديدة
| المسار داخل المشروع | الوصف |
|---------------------|-------|
| `platform/common/touch/tpd_himax_spi.c` | سائق HX83102D على SPI2 |
| `platform/common/spi/rules.mk` | ربط وحدة SPI العامة بالبناء |
| `platform/common/boot_menu/fastboot_ui.c` | واجهة Fastboot بهوية NovaLK |

### معدّلة
| المسار داخل المشروع | التغيير |
|---------------------|---------|
| `platform/common/touch/tpd.c` | SPI أولاً + توجيه القراءة حسب الناقل + اسم `HX83102D` |
| `platform/common/touch/tpd_himax.c` | تصحيح نبضة reset (تنتهي released) |
| `platform/common/touch/tpd_himax.h` | تعريفات SPI |
| `platform/common/touch/rules.mk` | إضافة `tpd_himax_spi.o` |
| `platform/common/spi/spi.c` | `init_spi_bus_by_reg()` + `arch/ops.h` |
| `platform/common/include/spi.h` | تعريف الدالة + تقصير المهلة |
| `platform/common/rules.mk` | `MODULES += spi` خلف `MTK_SPI_SUPPORT` |
| `platform/common/include/boot_menu.h` | تعريفات واجهة Fastboot وحالة USB |
| `platform/common/boot_menu/rules.mk` | إضافة `fastboot_ui.o` |
| `project/rmx2180.mk` | `MTK_SPI_SUPPORT = yes` |
| `app/mt_boot/fastboot.c` | تحديث بطاقة USB عند online/offline |
| `target/rmx2180/init.c` | رسم شاشة Fastboot في `target_fastboot_init()` |
| `target/rmx2180/include/target/cust_tpd.h` | تحديث التوثيق (SPI2 وليس I2C) |
| `include/dev/tpd.h` | تحديث التوثيق |

---

## 4. البناء

```
make rmx2180
```

لا حاجة لأي خطوة إضافية: `MTK_SPI_SUPPORT=yes` و`MTK_TPD_SUPPORT=yes`
موجودان في `project/rmx2180.mk`.

اختبارات الصياغة (host gcc) تمّت على: `tpd_himax_spi.c`, `tpd.c`,
`tpd_himax.c`, `spi.c`, `fastboot_ui.c`, `boot_menu.c` — بدون أخطاء.

---

## 5. ما يجب أن يظهر في سجل LK

عند الإقلاع إلى قائمة NovaLK:

```
tpd: spi2 cs0 id 0x83102d (no reset)
tpd: HX83102D ready on spi2 cs0 (id 0x83102d)
[BOOTMENU] touch: HX83102D
```

أو إذا كانت الشريحة في reset أثناء الإقلاع:

```
tpd: spi2 cs0 no answer, reset pulse
tpd: spi2 cs0 id 0x83102d after reset (25 ms)
tpd: HX83102D ready on spi2 cs0 (id 0x83102d)
```

عند أول لمسة:

```
tpd: first frame cnt 1 x 360 y 812 (raw 01 68 03 2c)
```

وفي وضع Fastboot:

```
tpd: ... (لا شيء إضافي)
```
وشاشة **FASTBOOT** تظهر، وتتحول بطاقة الحالة إلى **USB CONNECTED** عند وصل الكابل.

### حالات فشل محتملة ومعناها
| السجل | المعنى | الخطوة التالية |
|-------|--------|----------------|
| `Timeout for spi status1 reg.` ثم `tpd: spi2 transfer failed, controller not clocked?` | ساعة SPI2 مغلقة أو الـ pinmux خاطئ | تفعيل بوابة ساعة SPI2 في INFRA_AO |
| `tpd: spi2 cs0 still silent after reset` | الناقل يعمل لكن الشريحة لا تجيب | التحقق من GPIO178 ومن تغذية الشريحة |
| `tpd: bad frame checksum ...` | الإطار يُقرأ لكنه غير سليم | تجربة مسار FIFO تلقائياً |
| `tpd: spi DMA event read failed, using FIFO frames` | DMA غير متاح، تم التحويل تلقائياً | لا شيء |

---

## 6. ملاحظات

* أزرار الصوت + الباور تعمل كما هي، وقائمة الإقلاع الثلاثية لم تتغير.
* إذا لم يتم اكتشاف اللمس يظهر في التذييل `touch not detected - use the keys`
  ويعمل كل شيء بالأزرار.
* كل المحتوى المأخوذ من السجل أو الـ DTS تعامل معه كبيانات فقط.
