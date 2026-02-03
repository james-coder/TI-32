const versionEl = document.getElementById("fw-version");
const chipEl = document.getElementById("fw-chip");
const statusEl = document.getElementById("fw-status");
const browserWarning = document.getElementById("browser-warning");
const copySteps = document.getElementById("copy-steps");

const hasSerial = "serial" in navigator;
if (browserWarning) {
  browserWarning.style.display = hasSerial ? "none" : "block";
}

async function loadManifest() {
  try {
    const response = await fetch("manifest.json", { cache: "no-store" });
    if (!response.ok) {
      throw new Error("Manifest not found");
    }
    const manifest = await response.json();
    versionEl.textContent = manifest.version || "dev";
    const build = manifest.builds && manifest.builds[0];
    if (build && build.chipFamily) {
      chipEl.textContent = build.chipFamily;
    }
    statusEl.textContent = hasSerial ? "Ready" : "Browser unsupported";
  } catch (err) {
    statusEl.textContent = "Manifest missing";
    statusEl.style.color = "#b45309";
  }
}

const bootSteps = `Enter bootloader:\n1) Hold BOOT\n2) Tap RESET\n3) Release BOOT`; 

if (copySteps) {
  copySteps.addEventListener("click", async () => {
    try {
      await navigator.clipboard.writeText(bootSteps);
      copySteps.textContent = "Copied!";
      setTimeout(() => {
        copySteps.textContent = "Copy Boot Steps";
      }, 2000);
    } catch (err) {
      copySteps.textContent = "Copy failed";
      setTimeout(() => {
        copySteps.textContent = "Copy Boot Steps";
      }, 2000);
    }
  });
}

loadManifest();
