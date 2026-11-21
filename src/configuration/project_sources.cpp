#include "project_sources.h"

#include "project_manifest.h"

namespace configuration {

std::vector<std::string> readSources(const std::filesystem::path& manifest) {
  return readManifest(manifest).sources;
}

bool writeSources(const std::filesystem::path& manifest, const std::vector<std::string>& sources) {
  Manifest current = readManifest(manifest);
  current.sources = sources;
  return writeManifest(manifest, current);
}

}