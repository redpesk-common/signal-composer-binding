#!/bin/sh -x
trap "cleanup 0" EXIT
trap "cleanup 1" SIGINT SIGTERM SIGABRT SIGHUP
cleanup() {
  trap '' SIGINT SIGTERM SIGABRT SIGHUP EXIT
  kill $AFTESTPID > /dev/null 2>&1
  pkill $PROCNAME
  exit $1
}

BINDER=$(command -v afb-daemon)
AFBTEST="$(pkg-config --variable libdir afb-test)/aft.so"
if [ ! $? -eq 0 ]; then echo "Set PKG_CONFIG_PATH for afb-test"; exit -1; fi
PROCNAME="afbd-signal-composer"
PORT=1234
TOKEN=
[ "$1" ] && BUILDDIR="$1" || exit 1
AFTESTSOCKET=/tmp/signal-composer

TESTPACKAGEDIR="${BUILDDIR}/package-test"
export AFT_CONFIG_PATH="${TESTPACKAGEDIR}/etc"
export AFT_PLUGIN_PATH="${TESTPACKAGEDIR}/var:${TESTPACKAGEDIR}/lib/plugins"

pkill $PROCNAME

${BINDER} 	--name="${PROCNAME}" \
            --port="${PORT}" \
			--tracereq=common \
			--token=${TOKEN} \
			--workdir="${BUILDDIR}" \
            --binding=${AFBTEST} \
			--binding="package/lib/afb-signal-composer.so" \
            -vvv \
            --call="aft-signal-composer/launch_all_tests:{}" \
            --call="aft-signal-composer/exit:{}"
AFTESTPID=$!

find "${BUILDDIR}" -name test_results.log -exec cat {} \;

