# Liero Mobile (Android)

Port para Android do **OpenLiero / Liero 1.36** (código C++ oficial de
[gliptic/liero](https://github.com/gliptic/liero), referenciado em
[liero.be](https://www.liero.be/)). O jogo em si é o código original; esta
camada adiciona plataforma Android, controles de toque/gamepad e um menu inicial.

## Estrutura

| Pasta | O quê |
|---|---|
| `game/src` | Código do OpenLiero (upstream `a2ed03c`) + `game/mobile/` (toque, gamepad, ciclo de vida Android). Mudanças no código original ficam atrás de `#ifdef LIERO_MOBILE` e não tocam nas regras. |
| `game/TC/openliero` | Dados do jogo (regras/armas originais, sprites, sons WTFPL). |
| `android/` | Projeto Gradle: SDL2 (compilado do fonte com NDK) + launcher Java. |
| `tests/sim` | Simulação determinística headless (fidelidade). |
| `tests/shots` | Build desktop do código mobile para capturas automáticas. |
| `tests/emulator.sh` | Instala o APK no emulador, joga com `adb input`, tira capturas. |

## CI (GitHub Actions)

- **Fidelity**: compila o simulador contra o OpenLiero *original sem modificações*
  e contra este port (gcc, clang e ARM64 via qemu) e compara um hash do estado
  completo do jogo (RNG, vermes, corda ninja, armas, todos os objetos, mapa e
  frame renderizado) em 8 cenários (~120 mil frames). Qualquer diferença falha.
- **Screenshots**: capturas em 3120×1440 (S25 Ultra), 2340×1080 e 1920×1080.
- **APK**: arm64-v8a, armeabi-v7a, x86_64. Assinatura: secrets `LIERO_KEYSTORE_B64`/`LIERO_KEYSTORE_PASSWORD` se existirem; senão uma chave gerada e guardada no cache do Actions.
- **Emulator**: roda o APK real, toca nos botões, testa Voltar/Home, salva logcat.
- **Release**: quando `VERSION` muda em `main` e existe `docs/release-notes/v<VERSION>.md`.

Licenças: ver [LICENSES.md](LICENSES.md).
