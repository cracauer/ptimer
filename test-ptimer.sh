#! /bin/sh

set -u
set -b

# set -x

function error () {
  printf "ERROR: '%s'\n" "$*" 1>&2
  echo last test-ptimer.report: 1>&2
  cat  test-ptimer.report 1>&2
  # later I can do count but for now just abort
  # on first error
  exit 2
}

function test_one_signal () {
  # pass in signal number without the "-"
  local sig="$1"
  echo pid $$ testing signal $sig
  [ -f test-ptimer.report ] && rm test-ptimer.report
  ./ptimer -o test-ptimer.report sh -c 'sleep 0.25; kill -'$sig' $$; exit 42' || true
  [ -f test-ptimer.report ] || sleep 1
  [ -f test-ptimer.report ] || sleep 2
  [ -f test-ptimer.report ] || sleep 3
  cat test-ptimer.report # would then exit test with failure if it exists
  if ! egrep -q '^0\.[1234].*full seconds real time' test-ptimer.report ; then
    error Wrong runtime
  fi
  if ! grep -q "^$sig command terminated by signal" test-ptimer.report ; then
    error Did not notice signal $sig
  fi
}

function test_regular () {
  # no signal
  echo testing no.signal:
  ./ptimer -o test-ptimer.report sh -c 'sleep 0.52 ; exit 42' || true
  if ! egrep -q '^0\.[4567].*full seconds real time' test-ptimer.report ; then
    error Wrong runtime
  fi
  if grep -q "command terminated by signal" test-ptimer.report ; then
    error Unexpected signal
  fi
  if ! grep -q '2 command terminated on its own' test-ptimer.report ; then
    error exit code not correct
  fi
}

if ! [ -x ptimer ] ; then
  echo I think you need to build ./ptimer 1>&2
  exit 3
fi

test_one_signal 15
test_one_signal 9
test_regular

echo Made it to end of script.  All tests seemed to have worked.
