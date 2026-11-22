# Build from source

On an up-to-date Omarchy or Arch Linux system:

```sh
git clone https://github.com/TheMarchitect/omafreewrite.git ~/omafreewrite
cd ~/omafreewrite
./bin/install
```

This installs what building needs (`base-devel`, `qt6-base`, `qt6-declarative`),
builds the app, makes the Arch package and installs it. It takes under a minute.
To update later, run `git pull` and `./bin/install` again.

Then set up local AI for Polish, if you want it:

```sh
./bin/setup-polish            # or --large on a 12 GB+ graphics card
```

The installed package carries the same script as `omafreewrite-setup-polish`.

Other scripts:

| Script | What it does |
| --- | --- |
| `./bin/build` | Builds `build/omafreewrite` without installing anything |
| `./bin/test` | Builds and runs the test suite offscreen |
| `./bin/setup-polish` | Installs Ollama and a Polish model, and points the app at it |

The version number is `pkgver` in [pkgbuild/PKGBUILD](pkgbuild/PKGBUILD).

## Making a release

1. Bump `pkgver` in `pkgbuild/PKGBUILD` and commit.
2. Build the package: `cd pkgbuild && makepkg -f`.
3. Tag and publish, attaching the package:

   ```sh
   gh release create v0.2.1 pkgbuild/omafreewrite-0.2.1-1-x86_64.pkg.tar.zst \
     --title "OmaFreewrite 0.2.1" --notes "What changed"
   ```

4. Update the version in the quick-install line in the README.
