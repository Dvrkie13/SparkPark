/* SparkPark's version, as its GitHub release is tagged (v0.3.0-alpha is
 * "0.3.0-alpha"). One place for it: the builder (builder/launcher.cpp)
 * writes it into build.json and offers a rebuild when the game was built
 * by another version, and the update check (update.c) compares SparkPark's
 * latest release with it. Upstream's own version (version.h) is left as
 * upstream has it, so merges don't conflict there. Raised with each
 * SparkPark release. */
#pragma once

#define SPARKPARK_VERSION "0.3.0-alpha"
#define SPARKPARK_REPO "Dvrkie13/SparkPark"
