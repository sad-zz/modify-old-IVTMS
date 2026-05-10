# PSU — Netlist (source of truth)

## Nets
| Net          | Pins                                    |
|--------------|-----------------------------------------|
| +12V_IN_RAW  | J1.1, F1.1                              |
| +12V_FUSED   | F1.2, D1.A                              |
| +12V_PROT    | D1.K, TVS1.1, C1.+, L1.1, LED1.A        |
| +12V_BUS     | L1.2, C2.+, U1.1 (Vin), J_BUS.1, J_BUS.2, TP1.1 |
| SW_NODE      | U1.2 (Output), D2.K, L2.1               |
| +5V          | L2.2, C3.+, C4.1, U1.4 (FB), J_BUS.5, J_BUS.6, TP2.1 |
| GND          | J1.2, C1.-, C2.-, C3.-, C4.2, U1.3 (GND), U1.5 (ON/OFF=tied to GND for always-on... actually leave open or pull-low), D2.A, TVS1.2, LED1.K via R3, LED2.K via R4, J_BUS.3, J_BUS.4, J_BUS.7, TP3.1 |
| LED_STATUS   | J_BUS.15, LED2.A via R4 (active-high from MCU) |

> توجه: LM2596S-5.0 (fixed) فقط 5 پین داره: 1=Vin, 2=Output, 3=GND, 4=FB (must connect to output), 5=ON/OFF (active-low ➜ به GND وصل برای always-on)

## Power flags (KiCad)
- PWR_FLAG on +12V_IN_RAW (input)
- PWR_FLAG on +5V (regulator output)
- PWR_FLAG on GND

## ERC checklist
- [ ] هر دو پین GND backplane به plane وصل
- [ ] FB pin (U1.4) فقط به +5V (نه قبل از inductor)
- [ ] D2 cathode به SW node، آند به GND
- [ ] TVS موازی با ورودی (نه سری)
