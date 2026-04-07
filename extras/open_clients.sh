#!/bin/bash
# Script de Layout: Torres de Control (Altas y Estrechas)

# --- CONFIGURACIÓN ---
# 50 columnas de ancho x 22 filas de alto (Ajusta el 22 si se salen por abajo)
SIZE="50x22"

# FILA SUPERIOR (1, 2, 3)
POS[1]="+0+0"      # Arriba Izquierda
POS[2]="+500+0"    # Arriba Centro
POS[3]="+1000+0"   # Arriba Derecha

# FILA INFERIOR (4, 5) - Bajamos la Y a +480 para que no se pisen
POS[4]="+0+480"    # Abajo Izquierda
POS[5]="+500+480"  # Abajo Centro
POS[6]="+1000+480" # aBAJO DERECHA

for i in {1..6}
do
    gnome-terminal --geometry="${SIZE}${POS[$i]}" --title="CHAT_$i" -- bash -c "{ 
        echo 'PASS pass'; 
        sleep 0.5; 
        echo 'NICK CHAT_$i'; 
        sleep 0.5; 
        echo 'USER chat$i 0 * :Bot$i'; 
        sleep 0.5; 
        echo 'JOIN #42'; 
        cat; 
    } | nc 127.0.0.1 6667" &
done
