import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = dirname(fileURLToPath(import.meta.url));

export default defineConfig({
  base: "/",
  plugins: [react()],
  resolve: {
    alias: {
      "@": resolve(root, "src"),
      "@domain": resolve(root, "src/domain")
    }
  },
  server: {
    host: "0.0.0.0",
    port: 5174,
    proxy: {
      "/api": { target: "http://192.168.1.143", changeOrigin: true },
      "/images": { target: "http://192.168.1.143", changeOrigin: true, rewrite: (path) => path.replace(/^\/images/, ""), },
      "/ws": { target: "ws://192.168.1.143", ws: true, changeOrigin: true }
    }
  },
  build: {
    outDir: "dist",
    emptyOutDir: true,
    target: "es2017",
    rollupOptions: {
      output: {
        entryFileNames: "assets/app-v2.js",
        chunkFileNames: "assets/chunk-[hash].js",
        assetFileNames: assetInfo =>
          assetInfo.names.some(name => name.endsWith(".css"))
            ? "assets/index-v2.css"
            : "assets/[name][extname]"
      }
    }
  }
});
