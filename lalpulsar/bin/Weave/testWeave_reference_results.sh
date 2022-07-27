if test "${CFITSIO_ENABLED}" = false; then
    echo "Skipping test: requires CFITSIO"
    exit 77
fi

# Require at least one test that compares against reference results to pass/skip

exitcode=1
for test in single_segment interpolating non_interpolating; do
    testlog="../testWeave_${test}.log"
    teststatus=`tail -n 1 ${testlog}`
    echo "${teststatus}"
    case "${teststatus}" in
        PASS*|SKIP*)
            exitcode=0
            ;;
    esac
done
exit ${exitcode}
