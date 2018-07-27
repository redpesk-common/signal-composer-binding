#!/bin/sh
trap "cleanup 0" EXIT
trap "cleanup 1" SIGHUP SIGINT SIGABRT SIGTERM
cleanup() {
  rm -f $LOGPIPE
  echo "Removing $LOGPIPE"
  trap '' EXIT SIGHUP SIGINT SIGABRT SIGTERM
  exit $1
}

BINDER=$(command -v afb-daemon)
AFBTEST="$(pkg-config --variable libdir afb-test)/aft.so"
PROCNAME="aft-signal-composer"
PORT=1234
TOKEN=
LOGPIPE="test.pipe"
[ "$1" ] && BUILDDIR="$1" || exit 1

TESTPACKAGEDIR="${BUILDDIR}/package-test"
[ ! -p $LOGPIPE ] && mkfifo $LOGPIPE
export AFT_CONFIG_PATH="${TESTPACKAGEDIR}/etc"
export AFT_PLUGIN_PATH="${TESTPACKAGEDIR}/var:${TESTPACKAGEDIR}/lib/plugins"

pkill $PROCNAME

${BINDER} 	--name="${PROCNAME}" \
			--port="${PORT}" \
			--roothttp=. \
			--tracereq=common \
			--token=${TOKEN} \
			--workdir="${TESTPACKAGEDIR}" \
			--binding="../package/lib/afb-signal-composer.so" \
			--binding="$AFBTEST" \
			--call="aft-signal-composer/launch_all_tests:{}" \
			--call="aft-signal-composer/exit:{}" \
			-vvv > ${LOGPIPE} 2>&1 &

while read -r line
do
	[ "$(echo "${line}" | grep 'NOTICE: Browser URL=')" ] && break
done < ${LOGPIPE}