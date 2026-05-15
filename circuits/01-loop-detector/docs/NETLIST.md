# LOOP Detector — Netlist (source of truth)

> این سند بازسازی **عیناً** مدار `main loop pcb3.SchDoc` از PDF است.
> برای هر یک از ۴ کانال (`xx ∈ {11, 12, 13, 14}`) همان توپولوژی تکرار شده است.

## توپولوژی هر کانال (الگوی واحد)

```
P1.k ──R##A(0R)──┬─C##B(47nF)──GND
                 ├─Q##A.D                         (2N7000 protection)
                 ├─Q##B.S → Q##B.D ─ GND
                 ├─Q##C.G                         (2N7000 cascade)
                 ├─C##A(47nF)── GND
                 └─D##A.A ── GND (D##A.K to LOOP_IN_xx)

LOOP_IN_xx ──R##C(330K)── VCC
LOOP_IN_xx ──R##B(10K)──┬─ U50/x.IN-
                        └─R##D(3.3K)── GND
                             (LM339 input bias)

U50/x.IN+ ── threshold ref (set per channel, see below)
U50/x.OUT ── R##E(1K) ── Q##D.G          (Q##D = 2SJ196 / BSS92)
Q##D.S ── VCC
Q##D.D ── pl.k
```

## جدول تخصیص LM339 (U50)

| Channel | LM339 quarter | OUT pin | IN+ pin | IN- pin |
|---------|---------------|---------|---------|---------|
| 11      | U50A          | 2       | 5       | 4       |
| 12      | U50B          | 1       | 7       | 6       |
| 13      | U50C          | 14      | 9       | 8       |
| 14      | U50D          | 13      | 10      | 11      |
| —       | U50.VCC = 3   | —       | —       | —       |
| —       | U50.GND = 12  | —       | —       | —       |

## جدول کامل nets

| Net          | Connected pins                                                                |
|--------------|-------------------------------------------------------------------------------|
| **VCC (+5V)**| J_BUS.5, J_BUS.6, U50.3, U51.16, U52.16, R11C.1, R12C.1, R13C.1, R14C.1, Q11D.S, Q12D.S, Q13D.S, Q14D.S |
| **GND**      | J_BUS.3, J_BUS.4, J_BUS.7, U50.12, U51.7, U51.8, U52.7, U52.8, all C##.−, all D##A.A, Q##A.S, Q##B.D, Q##C.S |
| **MUX_A**    | J_BUS.8, U51.11, U52.11                                                       |
| **MUX_B**    | J_BUS.9, U51.10, U52.10                                                       |
| **MUX_C**    | J_BUS.10, U51.9, U52.9                                                        |
| **MUX_INH**  | J_BUS.11, U51.6, U52.6                                                        |
| **LOOP_IO_COM_U51** | J_BUS.12, U51.3                                                       |
| **LOOP_IO_COM_U52** | J_BUS.13, U52.3                                                       |
| **CMP_OUT**  | J_BUS.14 — wired-OR از خروجی Q##D ها (همه به یک خط، open-drain logic)         |

### ورودی‌ها (هدر P1, 8-pin)
| P1 pin | Net          | Channel use     |
|--------|--------------|-----------------|
| 1      | LOOP_IN_RAW_11 | کانال 1 سیم a |
| 2      | LOOP_IN_RAW_11_R | کانال 1 سیم b (return — به GND) |
| 3      | LOOP_IN_RAW_12 | کانال 2 a |
| 4      | LOOP_IN_RAW_12_R | کانال 2 b |
| 5      | LOOP_IN_RAW_13 | کانال 3 a |
| 6      | LOOP_IN_RAW_13_R | کانال 3 b |
| 7      | LOOP_IN_RAW_14 | کانال 4 a |
| 8      | LOOP_IN_RAW_14_R | کانال 4 b |

### خروجی‌ها (هدر pl, 8-pin)
| pl pin | Net          | Notes |
|--------|--------------|-------|
| 1      | LOOP_OUT_11  | از Q11D.D — به ADC/digital MCU |
| 2      | LOOP_OUT_12  | از Q12D.D |
| 3      | LOOP_OUT_13  | از Q13D.D |
| 4      | LOOP_OUT_14  | از Q14D.D |
| 5–8    | reserved     | اختیاری برای توسعه |

### اتصالات MUX به کانال‌ها
| MUX pin | U51 (channel) | net      |
|---------|---------------|----------|
| 13 (IO0)| 0             | LOOP_IN_11 |
| 14 (IO1)| 1             | LOOP_IN_12 |
| 15 (IO2)| 2             | LOOP_IN_13 |
| 12 (IO3)| 3             | LOOP_IN_14 |
| 1 (IO4) | 4             | LOOP_OUT_11 |
| 5 (IO5) | 5             | LOOP_OUT_12 |
| 2 (IO6) | 6             | LOOP_OUT_13 |
| 4 (IO7) | 7             | LOOP_OUT_14 |

(U52 می‌تونه برای aux channels یا آینده reserved بمونه — در شماتیک اصلی موازی بسته شده.)

## شمارهٔ ارجاع کامل قطعات (per channel)

| Ref     | Value          | Footprint                                  |
|---------|----------------|--------------------------------------------|
| C##A    | 47nF X7R       | C_0805                                     |
| C##B    | 47nF X7R       | C_0805                                     |
| R##A    | 0R             | R_0805                                     |
| R##B    | 10K 1%         | R_0805                                     |
| R##C    | 330K 1%        | R_0805                                     |
| R##D    | 3.3K 1%        | R_0805                                     |
| R##E    | 1K 1%          | R_0805                                     |
| Q##A    | 2N7000         | SOT-23                                     |
| Q##B    | 2N7000         | SOT-23                                     |
| Q##C    | 2N7000         | SOT-23                                     |
| Q##D    | 2SJ196 / BSS92 | SOT-23 (BSS92 SMD معادل)                    |
| D##A    | 1N5819         | SOD-123                                    |

و یکبار (نه per-channel):
| U50     | LM339          | SOIC-14                                    |
| U51, U52| HCF4051BEY     | SOIC-16                                    |
| P1      | Header 8       | PinHeader_1x08_2.54mm                      |
| pl      | Header 8       | PinHeader_1x08_2.54mm                      |
| J_BUS   | Header 2x8     | PinHeader_2x08_2.54mm                      |

## خلاصه شمارش
- مقاومت: 5 × 4 = 20
- خازن: 2 × 4 = 8
- ترانزیستور: 4 × 4 = 16 (12× 2N7000 + 4× 2SJ196)
- دیود: 4 × 1N5819
- IC: 1× LM339 + 2× HCF4051
- هدر: P1 + pl + J_BUS
