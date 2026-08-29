# Serves the generated documentation.
#
# The build context is the GENERATED tree, not the source tree: the
# `docs` target copies this file, nginx.conf and .dockerignore into
# ${CMAKE_BINARY_DIR}/docs beside the HTML, so that directory is a
# complete context on its own. It holds only what is served — the
# build's intermediates live beside it in docs-build/.
#
#     cmake --build build --target docs
#     docker build -t spellcircle-docs build/docs
#     docker run --rm -p 8080:80 spellcircle-docs
#
# Doxygen is not in this image and does not need to be. The pages are
# static files by the time they get here, so the image carries no
# toolchain and nothing in it can regenerate a page.

FROM nginx:1.29-alpine

# Replaces the default server block rather than adding a second one:
# the stock config already serves /usr/share/nginx/html on port 80, and
# two servers listening on the same port makes which one answers depend
# on include order.
COPY nginx.conf /etc/nginx/conf.d/default.conf

COPY . /usr/share/nginx/html/

# The image is only useful if it can actually answer, and a container
# that has stopped serving while still running is the failure this
# catches. Static files mean the check is cheap.
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s \
  CMD wget -q -O /dev/null http://localhost/index.html || exit 1

EXPOSE 80
