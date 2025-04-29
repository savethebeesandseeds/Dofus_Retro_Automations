# Dofus‑Retro Automation Toolkit

I know, instructions could be easier. All you need to know is that this is a single file .exe that makes the bot. 
Its build to run as configured in `.config` file and folder `procedures/*.proc`. 

To run just, configure `.config` and then do:

```cmd
test_proc.exe
```

It's a bot, to automate Game's Jobs or Actions.

**Automate Dofus Retro** with computer vision, OCR and a declarative scripting
language – all in a single portable executable.

* **Language:** Modern C++17  
* **Vision / OCR:** OpenCV 4 + Tesseract 5  
* **Input:** Win32 (mouse / keyboard / window capture)  
* **Build:** MinGW‑w64 (cross‑compiled from Linux)  
* **Host OS:** Windows 10/11 (x64)  

---

## 1. Why this project exists

Grinding markets and professions in **Dofus Retro** is fun… until it isn’t.
This toolkit lets you record that loop once as a tiny **`.proc`** file and have
the bot repeat it flawlessly while you do better things (or play the game’s
fun parts).

---

## 2. Feature tour

| Area                       | Details |
|----------------------------|---------|
| Screen capture             | DXGI / BitBlt fallback, DPI‑aware |
| Region detection           | Fast HSV filter for consistent orange highlights |
| OCR                        | Tesseract with per‑window language packs |
| Scripting engine           | `set`, `sleep`, `goto`, `call_fn`, `call_proc`, loops |
| Intrinsics                 | Add new C++ functions in **`include/dproc_fn.hpp`** and expose them instantly |
| Logging                    | Colour log levels, JSON dump of captured variables |
| Cross‑build CI             | Single Docker line builds a statically‑linked **`test_proc.exe`** |

---

## 3. Directory structure

```text
.
├─ include/            # headers (config, logging, Win32, OpenCV helpers, intrinsics)
├─ src/                # implementation
├─ procedures/         # your .proc scripts
├─ tests/              # unit + integration tests (entry point: test_proc.cpp)
├─ build/              # where static OpenCV / Tesseract libs live (git‑ignored)
└─ makefile
```

---

## 4. Quick build (recommended)

```bash
docker run --rm -it -v %cd%:/src ghcr.io/dofus-retro/ci:latest   bash -c "./scripts/bootstrap.sh && make test64 -j$(nproc)"
```

When it finishes you get **`bin/test_proc.exe`** ready to copy to Windows.

The `ghcr.io/dofus-retro/ci` image already bundles MinGW‑w64, CMake ≥ 3.22 and
static OpenCV / Tesseract builds, so no dependency hunting.

---

## 5. Manual build (if you insist)

<details>
<summary>Click to expand</summary>

1. **Container**  
   ```bash
   docker pull debian:11
   docker run --name dofus_retro -it -v %cd%:/src debian:11
   ```

2. **Packages**  
   ```bash
   apt update && apt install -y        git make cmake g++ mingw-w64 python3 pkg-config        --no-install-recommends
   ```

3. **Thread shim**  
   ```bash
   git clone https://github.com/meganz/mingw-std-threads /external/mingw-std-threads
   ```

4. **Static OpenCV / Tesseract**  
   Follow  
   https://github.com/savethebeesandseeds/opencv_mingw  
   https://github.com/savethebeesandseeds/tesseract_mingw  

   Both scripts drop their artefacts in `build/`.

5. **Make**  
   ```bash
   make test64 -j$(nproc)
   ```
</details>

---

## 6. Running the bot

1. Inspect file **`.confg`** . Change the Name of the Character, and the proc you want to run. 
2. Launch Dofus-Retro.
3. Open *cmd* and run:

   ```cmd
   test_proc.exe
   ```
4. Construct your own procedures/*.proc

---

## 7. Writing `.proc` files

```proc
# ► farm_loop.proc
set    target_resource   "Ash Wood"
call_proc  harvest_area        "$target_resource"
call_fn    click_next_item_in_line   20 300 200 30  15 0
sleep 200
call_proc  bank_run
goto   0         # infinite loop
```

### Built‑in commands

| Command | Purpose |
|---------|---------|
| `set k v`                  | store a variable |
| `sleep ms`                 | wait (ms) |
| `call_proc name …args`     | run another `.proc` file |
| `call_fn name …args`       | invoke a C++ intrinsic |
| `goto N`                   | jump to line N (zero‑based) |

See **`tests/test_proc.cpp`** for live examples.

---

## 8. Extending the bot

1. Add a function in **`include/dproc_fn.hpp`**:

   ```cpp
   bool heal(Context& ctx, const std::vector<std::string>&) {
       dw::press(ctx.hwnd, 'F1');
       return true;
   }
   REGISTER_FN(heal);
   ```

2. Re‑compile.
3. Use it inside your script:

   ```proc
   call_fn heal
   ```

---

## 9. Troubleshooting

| Symptom | Fix |
|---------|-----|
| Black screenshots | Disable *“Capture Windows & Games”* in Xbox Game Bar |
| Window scale issues | Right‑click `Dofus Retro.exe` → *Compatibility* → disable DPI scaling |
| Tesseract missing langs | Check `dconfig.cfg` → `languages_path` |
| `proc recursion too deep` | Add `return false` in your script loop |

---

## 10. Contributing

* Fork, create topic branch, commit with conventional messages.  
* `make test && clang-format -i src/**/*.cpp` before PR.  
* Keep CI green.

---

## 11. License

MIT © 2025 **Santiago “Waajacu” Restrepo** & contributors.

> *For beauty, community and a touch of retro nostalgia.*
