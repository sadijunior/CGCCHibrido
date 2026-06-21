# Trabalho GB — Minecraft

Visualizador 3D interativo com tema Minecraft feito em C++ com OpenGL 4.1 Core.

## Como funciona

A cena é definida por arquivos externos de texto que são lidos na inicialização e mapeados em structs internas. O arquivo `diorama.txt` descreve a cena inteira: posição da câmera, as três luzes e cada objeto com seu modelo, posição, escala e trajetória. Os modelos são carregados de `.obj` com seus materiais `.mtl`, e as trajetórias Bézier são lidas de arquivos `.txt` com quatro pontos de controle.

```
assets/
  Cenas/diorama.txt         — configuração da cena
  Modelos/                  — arquivos .obj e .mtl
  Trajetorias/              — pontos de controle Bézier (.txt)
```

## O que foi implementado

- **Camera FPS** — movimentação com WASD e rotação com o mouse, calculada via yaw/pitch
- **Iluminação Phong** com três fontes de luz (Key, Fill, Back), incluindo componentes ambiente, difusa e especular lidas do `.mtl` (Ka, Kd, Ks, Ns)
- **Curvas de Bézier cúbica** para animação de trajetória dos objetos, com reparametrização por comprimento de arco para velocidade aproximadamente constante
- **Transformações manuais** de translação, rotação e escala por objeto selecionado
- **Parser de cena** que configura câmera, luzes e objetos sem recompilar

## Controles

| Tecla | Ação |
|---|---|
| W / A / S / D | Mover câmera |
| Mouse | Rotacionar câmera |
| TAB | Alternar objeto selecionado |
| R / T / M | Modo rotação / translação / escala |
| Setas + I / J | Translação no eixo selecionado |
| X / Y / Z | Rotação contínua ou escala por eixo |
| [ / ] | Escala uniforme |
| 1 / 2 / 3 | Ligar/desligar luzes Key, Fill, Back |
| 7 / 8 / 9 | Ligar/desligar componente Ambiente, Difusa, Especular |
| V | Ativar/pausar trajetória Bézier do objeto |
| ESC | Sair |

## Compilação

```bash
cd build
cmake --build .
./MinecraftGB
```
