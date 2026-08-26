function sanitizeFileName(name: string): string {
  const leaf = name.replace(/\\/g, "/").split("/").pop() || "loco-image";
  return leaf.replace(/[^a-zA-Z0-9._-]/g, "_");
}

export async function uploadLocoImage(file: File): Promise<string> {
  const safeName = sanitizeFileName(file.name);
  const uploadFile = safeName === file.name
    ? file
    : new File([file], safeName, { type: file.type });

  const formData = new FormData();
  formData.append("file", uploadFile, safeName);

  const response = await fetch("/upload", {
    method: "POST",
    body: formData,
  });

  if (!response.ok) {
    throw new Error(`Image upload failed: HTTP ${response.status}`);
  }

  // Store a relative path in locos.json. From both the ESP32 UI and
  // Vite dev server this resolves to /images/<name>.
  return `images/${safeName}`;
}
