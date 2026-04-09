# Grotto Chat Client - tarkka build-ohje puhtaalle koneelle

Tama ohje on kirjoitettu tilanteeseen, jossa koneessa on juuri asennettu kayttojarjestelma eika valmiita C++ build-tyokaluja ole oletettavasti asennettu.

Ohje kattaa ainakin:

- Arch Linux
- Ubuntu
- Windows

Ohje perustuu taman repoon nykyiseen build-polkuun:

- build-järjestelma on `CMake`
- riippuvuudet tulevat `vcpkg.json`-manifestista
- osa riippuvuuksista haetaan `FetchContent`-mekanismilla suoraan buildin aikana
- valmis binaari tarvitsee myos buildin kopioimat oheistiedostot (`client.toml`, `help/`, `resources/`, `README.md`)

## Mita build oikeasti tekee

Kun ajat taman clientin buildin ensimmaisen kerran, tapahtuu kaytannossa tama:

1. CMake lukee projektin `CMakeLists.txt`-tiedoston.
2. vcpkg asentaa kaikki `vcpkg.json`:ssa maaritellyt C/C++-riippuvuudet.
3. CMake hakee buildin aikana myos ainakin nama upstream-projektit:
   - `signalapp/libsignal-protocol-c`
   - `mackron/miniaudio`
   - `nothings/stb`
4. Lopuksi CMake rakentaa `grotto-client`-binaarin.
5. Build kopioi binaarin viereen myos kaynnistykseen tarvittavia tiedostoja.

Taman takia ensimmainen build on hidas verrattuna seuraaviin build-kertoihin. Verkkoa tarvitaan ensimmaiseen configure/build-ajoon.

## Yhteiset minimivaatimukset kaikilla alustoilla

Tarvitset aina:

- Gitin
- CMake 3.20 tai uudemman
- C++20-yhteensopivan compilerin
  - Windows: MSVC / Visual Studio 2022 Build Tools
  - Linux: GCC 11+ tai Clang 13+
- Toimivan internet-yhteyden ensimmaiseen buildiin
- vcpkg:n

Repossa client sijaitsee polussa:

```text
grotto-chat-client
```

Kaikki alla olevat build-komennot ajetaan tassa hakemistossa, ellei toisin sanota.

## Valmis build onnistui, kun nama loytyvat

Linuxissa:

```text
build/grotto-client
build/client.toml
build/help/
build/resources/
```

Windowsissa:

```text
build\Release\grotto-client.exe
build\Release\client.toml
build\Release\help\
build\Release\resources\
```

Jos binaari valmistuu mutta `client.toml`, `help` tai `resources` puuttuvat, ala kaynnista clientia muualta kuin buildin tuottamasta output-hakemistosta.

## Arch Linux

### 1. Paivita jarjestelma

```bash
sudo pacman -Syu
```

### 2. Asenna build- ja bootstrap-tyokalut

Tama pakettirivi nojaa kahteen asiaan:

- repossa tarvitaan compiler + CMake + Git
- vcpkg:n oma bootstrap-skripti suosittelee Archille nimenomaan naita tyokaluja

```bash
sudo pacman -S --needed base-devel git cmake ninja curl zip unzip tar
```

### 3. Luo tyohakemistot

```bash
mkdir -p "$HOME/src"
cd "$HOME/src"
```

### 4. Hae ja bootstrapaa vcpkg

```bash
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
```

Kun bootstrap onnistuu, hakemistossa pitaa olla suoritettava tiedosto:

```bash
ls -l "$HOME/src/vcpkg/vcpkg"
```

### 5. Aseta `VCPKG_ROOT`

Nykyiseen shelliin:

```bash
export VCPKG_ROOT="$HOME/src/vcpkg"
```

Jos haluat saman pysyvasti:

```bash
echo 'export VCPKG_ROOT="$HOME/src/vcpkg"' >> ~/.bashrc
echo 'export PATH="$VCPKG_ROOT:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

### 6. Hae repo

```bash
cd "$HOME/src"
git clone https://github.com/hittoSepi/grotto.chat.git
cd grotto.chat/grotto-chat-client
```

### 7. Konfiguroi projekti

Kaytan tassa `Ninja`-generaattoria, koska se on suoraviivainen puhtaalta Linux-koneelta:

```bash
cmake -S . -B build \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
```

Jos tama vaihe onnistuu, build-hakemisto syntyy ja CMake loytaa vcpkg:n tyokaluketjun.

### 8. Rakenna client

```bash
cmake --build build
```

### 9. Tarkista tulos

```bash
ls -l build/grotto-client
ls -l build/client.toml
ls -ld build/help build/resources
```

### 10. Kaynnista client

```bash
./build/grotto-client
```

## Ubuntu

Alla oleva polku toimii tyypillisella uudella Ubuntu-asennuksella.

### 1. Paivita pakettilistat

```bash
sudo apt update
```

### 2. Asenna compiler, build-tyokalut ja vcpkg:n bootstrap-vaatimukset

```bash
sudo apt install -y \
  build-essential \
  git \
  cmake \
  ninja-build \
  curl \
  zip \
  unzip \
  tar \
  pkg-config
```

### 3. Luo tyohakemisto

```bash
mkdir -p "$HOME/src"
cd "$HOME/src"
```

### 4. Hae ja bootstrapaa vcpkg

```bash
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
```

Tarkista, etta vcpkg syntyi:

```bash
ls -l "$HOME/src/vcpkg/vcpkg"
```

### 5. Aseta `VCPKG_ROOT`

Nykyiseen shelliin:

```bash
export VCPKG_ROOT="$HOME/src/vcpkg"
```

Pysyvasti:

```bash
echo 'export VCPKG_ROOT="$HOME/src/vcpkg"' >> ~/.bashrc
echo 'export PATH="$VCPKG_ROOT:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

### 6. Hae repo

```bash
cd "$HOME/src"
git clone https://github.com/hittoSepi/grotto.chat.git
cd grotto.chat/grotto-chat-client
```

### 7. Konfiguroi projekti

```bash
cmake -S . -B build \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
```

### 8. Rakenna client

```bash
cmake --build build
```

### 9. Tarkista tulos

```bash
ls -l build/grotto-client
ls -l build/client.toml
ls -ld build/help build/resources
```

### 10. Kaynnista client

```bash
./build/grotto-client
```

## Windows

Windowsilla kannattaa tehda tama rauhassa oikeassa jarjestyksessa. Isoin syy build-virheisiin puhtaalla koneella on se, etta Visual Studio Build Tools puuttuu tai build-komento ajetaan vaarasta shellista.

### 1. Asenna Git

Asenna Git for Windows viralliselta sivulta:

- https://git-scm.com/download/win

Perusasennus riittaa.

### 2. Asenna CMake

Asenna CMake viralliselta sivulta:

- https://cmake.org/download/

Asennuksessa kannattaa valita vaihtoehto, joka lisaa CMake:n PATHiin.

### 3. Asenna Visual Studio 2022 Build Tools

Asenna Build Tools viralliselta sivulta:

- https://visualstudio.microsoft.com/downloads/

Valitse:

- `Tools for Visual Studio`
- `Build Tools for Visual Studio 2022`

Visual Studio Installerissa valitse workload:

- `Desktop development with C++`

Varmista, etta ainakin nama tulevat mukaan:

- MSVC v143 build tools
- Windows 10 SDK tai Windows 11 SDK
- C++ CMake tools for Windows

### 4. Avaa oikea shell

Avaa joko:

- `Developer PowerShell for VS 2022`
- tai `x64 Native Tools Command Prompt for VS 2022`

Tama on turvallisin tapa varmistaa, etta MSVC-tyokalut ovat varmasti kaytossa.

### 5. Hae ja bootstrapaa vcpkg

PowerShell-esimerkki:

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat
```

Kun tama onnistuu, tarkista:

```powershell
Get-Item C:\vcpkg\vcpkg.exe
```

### 6. Aseta `VCPKG_ROOT`

Nykyiseen shelliin:

```powershell
$env:VCPKG_ROOT = "C:\vcpkg"
```

Halutessasi pysyvasti kayttajalle:

```powershell
[Environment]::SetEnvironmentVariable("VCPKG_ROOT", "C:\vcpkg", "User")
```

Jos asetat muuttujan pysyvasti taman komennon avulla, avaa sen jalkeen uusi shell.

### 7. Hae repo

```powershell
New-Item -ItemType Directory -Force C:\src | Out-Null
cd C:\src
git clone https://github.com/hittoSepi/grotto.chat.git
cd C:\src\grotto.chat\grotto-chat-client
```

### 8. Konfiguroi projekti

Kayta eksplisiittisesti Visual Studio 2022 -generaattoria:

```powershell
cmake -S . -B build `
  -G "Visual Studio 17 2022" `
  -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
```

Jos tama vaihe onnistuu, hakemistoon syntyy mm. `build\grotto-client.sln`.

### 9. Rakenna client

```powershell
cmake --build build --config Release --target grotto-client
```

### 10. Tarkista tulos

```powershell
Get-Item .\build\Release\grotto-client.exe
Get-Item .\build\Release\client.toml
Get-ChildItem .\build\Release\help
Get-ChildItem .\build\Release\resources
```

### 11. Kaynnista client

```powershell
.\build\Release\grotto-client.exe
```

## Repon mukana tulevat shortcut-skriptit

Kun `VCPKG_ROOT` on asetettu oikein, voit kayttaa myos repossa olevia helper-skripteja:

Linux:

```bash
./build.sh
```

Tai build + testit yhdella komennolla:

```bash
./build.sh check
```

Windows:

```powershell
.\build.cmd
```

Tai build + testit yhdella komennolla:

```powershell
.\build.cmd check
```

Tarkeaa:

- molemmat skriptit nimeavat vanhan `build`-hakemiston muotoon `build-old`, `build-old-1`, jne.
- ne eivat tee inkrementaalista buildia olemassa olevaan `build`-hakemistoon
- Linux-skripti tekee `Release`-buildin `-DCMAKE_BUILD_TYPE=Release`
- Windows-skripti buildaa `Release`-konfiguraation
- `check`-argumentti ajaa kanonisen CMake-targetin, joka rakentaa kaikki rekisteroidyt testit ja suorittaa `ctest --output-on-failure`

Jos haluat hallita buildia tarkasti tai debugata configure-vaihetta, kayta mieluummin suoria `cmake`-komentoja kuin skripteja.

## Ensimmaisen buildin jalkeen

Kun build on onnistunut, kaytannollinen ajopolku on:

Linux:

```bash
cd build
./grotto-client
```

Windows:

```powershell
cd build\Release
.\grotto-client.exe
```

Syy tahan on se, etta build kopioi configin, help-tiedostot ja resurssit juuri output-hakemistoon.

## Yleisimmat build-ongelmat

### `VCPKG_ROOT` puuttuu tai toolchain-filea ei loydy

Oire:

- CMake ei loyda vcpkg:ta
- `Could not find a package configuration file`
- `CMAKE_TOOLCHAIN_FILE` viittaa olemattomaan polkuun

Ratkaisu:

- varmista etta vcpkg on oikeasti bootstrapattu
- varmista etta `VCPKG_ROOT` osoittaa oikeaan hakemistoon
- tarkista etta polku `.../scripts/buildsystems/vcpkg.cmake` on olemassa

### Compiler puuttuu

Linux-oire:

- `No CMAKE_CXX_COMPILER could be found`

Ratkaisu:

- Arch: varmista etta `base-devel` on asennettu
- Ubuntu: varmista etta `build-essential` on asennettu

Windows-oire:

- CMake ei loyda MSVC:ta
- build kaatuu heti generaattorin tai compilerin tunnistukseen

Ratkaisu:

- asenna `Desktop development with C++`
- kayta `Developer PowerShell for VS 2022` tai `x64 Native Tools Command Prompt for VS 2022`

### `git` puuttuu buildin aikana

Repossa kaytetaan `FetchContent`-hakua, joten build ei ole pelkka paikallinen compile ilman verkkoa.

Ratkaisu:

- asenna Git
- varmista etta `git` toimii shellissa ennen `cmake`-configurea

### Build-hakemisto on mennyt sekaisin

Jos olet vaihtanut generaattoria, compileria tai vcpkg-polkuja kesken kaiken, tee puhdas configure uudelleen.

Linux:

```bash
rm -rf build
```

Windows:

```powershell
Remove-Item -Recurse -Force .\build
```

Sen jalkeen aja configure ja build uudestaan.

## Viitteet

Ulkoiset viitteet, joiden mukaan tama ohje on tarkennettu:

- vcpkg CMake quick start: https://learn.microsoft.com/en-us/vcpkg/get_started/get-started
- WinGet install-komennon syntaksi: https://learn.microsoft.com/en-us/windows/package-manager/winget/install
- vcpkg bootstrap-scriptin Linux-prereqit: paikallisen vcpkg-asennuksen `scripts/bootstrap.sh`
