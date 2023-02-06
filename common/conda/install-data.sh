#!/bin/bash

set -e

_make="make -j ${CPU_COUNT} V=1 VERBOSE=1"

cd _build

# install data files only
${_make} -C lib install-pkgdataDATA

# install activate/deactivate scripts
for action in activate deactivate; do
	mkdir -p ${PREFIX}/etc/conda/${action}.d
	for ext in sh csh; do
		_target="${PREFIX}/etc/conda/${action}.d/${action}-${PKG_NAME}.${ext}"
		echo "-- Installing: ${_target}"
		cp "${RECIPE_DIR}/${action}-${PKG_NAME}.${ext}" "${_target}"
	done
done
