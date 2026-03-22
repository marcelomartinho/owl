# Owl Browser - Chat Session Log

Session: 2026-03-22
Session ID: session_01L4VM9DyA1H4eqJxhQaVwag

## Objetivo Original

Criar um novo browser baseado no Chromium focado em performance e eficiência no uso de memória, com suporte a extensões do Chrome Web Store.

## Decisão de Arquitetura

### Abordagem Inicial Descartada
- Electron como wrapper do Chromium (protótipo rápido mas superficial)

### Abordagem Final Aprovada
- **Fork direto do Chromium** com modificações no motor Blink
- Foco em reescrever/otimizar o sistema de gestão de memória do Blink
- Baseado no código aberto do Blink: https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/

## Pesquisa de Motores de Renderização

### Arquitetura de Memória do Blink (Chromium)
O Blink usa 4 alocadores:
1. **Oilpan (GC)** - Objetos DOM, grafos complexos (`/third_party/blink/renderer/platform/heap/`)
2. **PartitionAlloc** - LayoutObjects, Strings, Vectors (`/base/allocator/partition_allocator/`)
3. **Discardable Memory** - Buffers descartáveis sob pressão
4. **malloc** - Legacy (desencorajado)

Oilpan GC:
- Objetos herdam de `GarbageCollected<T>`
- Mark-sweep com sweeping incremental/concurrent
- Smart pointers: `Member<T>`, `Persistent<T>`, `WeakMember<T>`
- Arquivos chave: `heap_allocator_impl.h`, `custom_spaces.h/cc`, `process_heap.h/cc`, `thread_state.h/cc`
- Oilpan extraído para V8 como biblioteca standalone desde Chromium M94

### Técnicas do Firefox/Gecko
| Técnica | Impacto |
|---|---|
| **Arena Allocator para Layout** (nsPresShell) | Elimina fragmentação, melhora cache locality |
| **Zombie Compartment Cleanup** (MemShrink) | Resolve leak de memória de abas fechadas (#1 bug do Firefox) |
| **LRU Tab Unloading** (Tab Unloader) | Previne crashes OOM |
| **madvise(MADV_DONTNEED)** (jemalloc custom) | RSS diminui mesmo com páginas mapeadas |
| **Allocation Audit "Clownshoes"** (MemShrink) | Um fix de 1 byte salvou 700MB em páginas pesadas |
| **Memory Reporter Framework** (about:memory) | Visibilidade total, detecta regressões |
| **Incremental GC 10ms target** (SpiderMonkey) | 60fps mantido durante GC |
| **Hybrid ref counting + cycle collector** | Simples para C++ interop |
| **jemalloc arena count tuning** | macOS: 44% menos latência no malloc |

MemShrink Project (2011-2013):
- Transformou Firefox de "memory leak" para competitivo
- 1.5 anos de foco em memória
- Zombie compartments era o bug #1
- "Clownshoes" = bugs de rounding de alocação (1 byte extra = dobro de memória)
- areweslimyet.com = dashboard público de regressão

### Técnicas do WebKit/Safari
| Técnica | Impacto |
|---|---|
| **IsoHeap (Type Isolation)** (bmalloc/libpas) | +2% perf, +8% memória, previne type confusion |
| **3-Tier Memory Pressure** (iOS Jetsam) | Proativo em vez de reativo |
| **Concurrent GC Riptide** (JavaScriptCore) | 5x melhor em latency benchmarks, pausas <1ms |
| **Pointer Poisoning** (Spectre mitigations) | Segurança sem branch checks |
| **Gigacages** | Partição de memória por tipo (ArrayBuffer, strings) |
| **Tab Freezing 3-5 tiers** | Frozen → Partially loaded → Discarded |

### Técnicas do Servo
| Técnica | Impacto |
|---|---|
| **Rust para parsers críticos** | Elimina ~70% dos bugs de segurança de memória |
| **Actor-based task isolation** | Zero data races, failures isoladas |
| **Ownership model** (sem GC) | Sem pausas de GC no compositor |

### Técnicas do Brave
| Técnica | Impacto |
|---|---|
| **FlatBuffers para Ad Blocker** | 75% menos memória no bloqueador |
| **Extension Memory Auditing** | Identifica memory hogs |
| **45MB default savings** | Todas as plataformas |

### Técnicas do Ladybird
- Conservative stop-the-world GC para DOM/HTML APIs
- Pivô para Rust em áreas security-sensitive (parser, image decoding, font)
- ~70% dos bugs sérios do Chrome são memory-safety

### Chromium 2024 Otimizações
- 10% performance via data structure layout reordering
- Oilpan GC expansion (mais tipos de malloc → Oilpan)
- Chrome Memory Saver: 3 modos (Standard/Balanced/Advanced)

## Modificações Implementadas no Owl

### 1. Oilpan GC Agressivo + Concurrent
- GC policy com 3 modos: foreground (concurrent, <10ms), background (5s), emergency (contínuo)
- Zombie compartment cleanup ao fechar/navegar aba
- 3-tier pressure: Warning 80% → Critical 90% → Emergency 95%
- madvise(MADV_DONTNEED) para shrinking imediato
- OwlLowPrioritySpace + OwlIsoHeapSpace

### 2. PartitionAlloc + Arena Allocator
- Arena allocator para layout data (inspirado Firefox nsPresShell)
- Liberação atômica ao fechar página + memory poisoning
- Allocation audit logging ("Clownshoes" detection)
- Purge agressivo com madvise
- Pools menores, decommit mais agressivo

### 3. IsoHeap Type Isolation
- Tipos críticos (Node, Document, EventTarget, Element) segregados
- +2% performance (cache locality) + previne type confusion

### 4. Frame Memory Management
- Frame freezing (timers, rAF, fetches, service workers)
- Zombie reference cleanup ao navegar
- Layer discarding + data structure layout optimization
- Budget de memória por frame

### 5. Tab Lifecycle (LRU + 3-Tier)
- LRU unloading com exceções (áudio, pinned, WebRTC, forms)
- Heavy tab detection para >11 abas
- owl://tabs page (inspirada Firefox about:unloads)

### 6. Memory Reporter Framework
- Hierárquico por subsistema
- owl://memory page com dark theme
- JSON export para CI
- Extension memory auditing

### 7. Ad Blocker Nativo
- FlatBuffers para filter lists (75% menos memória)
- mmap para zero desserialização
- Network-level blocking via webRequest

### 8. UI Customizada
- Tab strip com indicador de memória (verde/amarelo/vermelho)
- Memory badge na toolbar
- Menu simplificado
- owl://memory e owl://tabs

## Arquivos Criados (46 arquivos, 4770 linhas)

### Motor Blink
- `src/third_party/blink/renderer/platform/heap/owl_gc_policy.h/cc`
- `src/third_party/blink/renderer/platform/heap/owl_arena_allocator.h/cc`
- `src/base/allocator/partition_allocator/owl_partition_config.h`

### Browser Process
- `src/chrome/browser/owl/owl_feature_flags.h`
- `src/chrome/browser/owl/owl_memory_pressure.h/cc`
- `src/chrome/browser/owl/owl_tab_discard_policy.h/cc`
- `src/chrome/browser/owl/owl_memory_monitor.h/cc`
- `src/chrome/browser/owl/adblocker/owl_request_filter.h/cc`
- `src/chrome/browser/owl/adblocker/owl_filter_list_manager.h/cc`
- `src/chrome/browser/owl/pages/owl_memory_page.h/cc`
- `src/chrome/browser/owl/pages/owl_tabs_page.h/cc`

### UI
- `src/chrome/browser/ui/views/owl/owl_browser_view.h/cc`
- `src/chrome/browser/ui/views/owl/owl_tab_strip.h/cc`
- `src/chrome/browser/ui/views/owl/owl_toolbar.h/cc`
- `src/chrome/browser/ui/views/owl/owl_memory_indicator.h/cc`
- `src/chrome/browser/ui/views/owl/owl_menu.h/cc`

### Build & Infra
- `owl_build/args.gn`
- `owl_build/owl_features.gni`
- `scripts/fetch_chromium.sh`
- `scripts/apply_patches.sh`
- `scripts/build.sh`
- `scripts/create_patches.sh`
- `scripts/memory_benchmark.sh`
- `src/chrome/browser/owl/BUILD.gn`
- `src/chrome/browser/owl/adblocker/BUILD.gn`
- `src/chrome/browser/ui/views/owl/BUILD.gn`

### Documentação
- `docs/architecture.md`
- `docs/building.md`
- `docs/memory-optimizations.md`

## Build System
- GN/Ninja (padrão do Chromium)
- Pré-requisitos: 100GB+ disco, 16GB+ RAM, Clang, Python 3.9+, depot_tools
- Feature flags via `buildflag_header` (OWL_AGGRESSIVE_GC, OWL_ISOHEAP_ENABLED, etc.)

## Extensões Chrome Web Store
- Suporte nativo (fork direto do Chromium)
- Manifest V2 e V3
- Sem modificações necessárias

## Metas de Performance
- 30-40% menos memória que Chrome vanilla
- GC pause <10ms (p99)
- Tab restore <500ms
- Speedometer: não mais que 5% mais lento que Chrome

## Próximos Passos Discutidos
- Criar protótipo Electron com instalador Windows para teste rápido
- Fase 9 (futura): Rust para parsers/decoders críticos (inspirado Servo/Ladybird)

## Fontes Pesquisadas
- Chromium Blink source: https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/
- Oilpan GC: https://www.chromium.org/blink/blink-gc/
- Oilpan library (V8): https://v8.dev/blog/oilpan-library
- Firefox MemShrink: https://aosabook.org/en/posa/memshrink.html
- Firefox Tab Unloading: https://hacks.mozilla.org/2021/10/tab-unloading-in-firefox-93/
- WebKit Riptide GC: https://webkit.org/blog/7122/introducing-riptide-webkits-retreating-wavefront-concurrent-garbage-collector/
- Brave adblock memory: https://brave.com/privacy-updates/36-adblock-memory-reduction/
- Servo: https://servo.org/
- Ladybird: https://github.com/LadybirdBrowser/ladybird
