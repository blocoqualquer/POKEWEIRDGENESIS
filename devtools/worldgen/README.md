# Ferramenta de teste do gerador de mundo

Compila o **mesmo** `src/worldgen.c` que vai para a ROM, mas no PC, para testar
e desenhar mundos sem precisar do emulador.

```sh
gcc -O2 -Wall -Wextra -std=gnu99 -iquote include -o worldgen_tool \
    devtools/worldgen/worldgen_tool.c src/worldgen.c

./worldgen_tool test 100000                 # confere as regras em 100 mil seeds
./worldgen_tool render mundo.ppm 2 12345    # desenha a seed 12345 (2 px por tile)
```

O CI roda o `test` a cada push. As regras verificadas: número de zonas entre 12 e
36, zonas dentro do mapa e sem sobreposição, aldeia inicial na zona 0, ao menos
2 castelos, todas as zonas ligadas por rota, zonas de rota só com segmentos retos,
as zonas de rota nunca atravessam as zonas que ligam, e a mesma seed sempre gera o mesmo mundo.
