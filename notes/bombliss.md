# Bombliss (modo 9, flow 0x10, client+0x578 = 0x13)

Regla de detonacion: NO esta en los scripts TPF2 (8044aa58 / 80466f08 no tienen comandos de bomba; Bombliss cae en 8044aa58 por
FUN_8005fda8). Es codigo nativo: CSingleResultWindow.c 0x801b3e64..0x801b4bb8, llamado cada frame sin pieza activa (FUN_80071124).
Implementado en game/bombliss.hpp (fiel salvo lo marcado [inferido]).

* Fila completa (todas las celdas ocupadas): sus bombas se encienden con nivel = nº de filas completas (max 20). Las filas NO se borran.
  Mecha 500 ms -> explosion (30 ms) -> se borra el area -> marca 200 ms -> desaparece.
* Area por nivel (tabla 0x804125f0, ancho x alto, centrada): 1:7x1 2:7x3 3:7x5 4:7x7 5-6:9x9 7-8:11x11 9-10:13x13 11-12:15x15 13+:17x17; bomba grande: 10x10 (-4..+5).
* Cadena: una bomba dentro del area se enciende (mecha 150 ms) heredando el nivel de la que la alcanzo; al terminar la oleada los bloques sin
  apoyo caen (grupos conexos, 128/45 por ms en 1/256 de celda) y se sube el bonus de cadena (+1 nivel por oleada); se reevaluan filas completas.
* 2x2 bombas pequeñas -> bomba grande (flags 6 ancla sup.izq. + 4 resto). Evento 0x36.
* Fase superada: ninguna celda ocupada (evento 0x35). Puntos = 99 - piezas fijadas (el resultado es el sobrante).
* Bomba por pieza (FUN_8006b170, bandera global r13-0x7e60 = 1 por defecto): celda aleatoria 1..4 (numeracion DAT_803ec2c0), y cada 8/11/15 piezas
  (dificultad por fase 0/1/2) una pieza entera de bombas. Todas las piezas color 0xc.
* Eventos/SE: 0x31 SE137 + destello de fila, 0x33 SE138 + explosion, 0x36 SE139, 0x34 destello por celda, 0x32 encendido.
## [inferido] / sin resolver
* Game over al llegar a 0 puntos (no localizado el origen exacto; en el original puede ser cola de piezas vacia). Gravedad nivel 1 TPF (el nivel nunca sube).
* Efectos: destellos procedurales en vez de expl_bomb_eff/expl_block_eff/ignite/bom_bom.brlan. Las bombas no se ven en la vista previa de NEXT.
* Pantalla de resultados, editor, total acumulado y records no implementados (solo hiscore de sesion). Sprite bomba: atlas (176,0,16,16), grande (192,0,32,32).
* FUN_801b5c20/modo 1 de FUN_801b3e64 (bombas como grupos tras garbage) no se usa: solo se activa con +0x544 bit 27 (ataques en VS).
