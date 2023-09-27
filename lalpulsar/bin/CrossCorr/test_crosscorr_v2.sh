##---------- names of codes and input/output files
mfd_code="lalpulsar_Makefakedata_v4"
pcc_code="lalpulsar_crosscorr_v2"

Ftolerance=0.05

# ---------- fixed parameter of our test-signal
Tsft=180;
startTime=827884814
duration=86400
endTime=827971214
refTime=827884814

# Sky coordinates of Sco X-1
alphaRad=4.2756992385
deltaRad=-0.272973858335

mfd_fmin=149.8
mfd_Band=0.4
mfd_h0=3e-22
mfd_cosi=0
mfd_psi=0
mfd_phi0=0
mfd_Freq=150.0
mfd_noiseSqrtSh=3e-23
mfd_seed1=201401090
mfd_seed2=201401091
mfd_ifo1=H1
mfd_ifo2=L1

pcc_fStart=149.9995
pcc_fBand=.001
pcc_maxLag=180
pcc_orbitAsiniSec=1.40
pcc_orbitAsiniSecBand=0.10
pcc_orbitPSec=68023.7136
pcc_orbitTimeAsc=1245967374
pcc_orbitTimeAscBand=20
pcc_numBins=1
pcc_mismatchF=0.1
pcc_mismatchA=0.1
pcc_mismatchT=0.1
pcc_mismatchP=0.1

mkdir -p sfts/

mfd_CL="--fmin=$mfd_fmin --Band=$mfd_Band --Freq=$mfd_Freq --outSFTbname=./sfts --noiseSqrtSh=$mfd_noiseSqrtSh --Alpha=$alphaRad --Delta=$deltaRad --Tsft=$Tsft --startTime=$startTime --duration=$duration --h0=$mfd_h0 --cosi=$mfd_cosi --psi=$mfd_psi --phi0=$mfd_phi0"
mfd_CL1="${mfd_CL} --IFO=$mfd_ifo1 --randSeed=$mfd_seed1"
mfd_CL2="${mfd_CL} --IFO=$mfd_ifo2 --randSeed=$mfd_seed2"

pcc_CL="--startTime=$startTime --endTime=$endTime --sftLocation='./sfts/*.sft' --fStart=$pcc_fStart --fBand=$pcc_fBand --alphaRad=$alphaRad --deltaRad=$deltaRad --maxLag=$pcc_maxLag --orbitAsiniSec=$pcc_orbitAsiniSec --orbitAsiniSecBand=$pcc_orbitAsiniSecBand --orbitPSec=$pcc_orbitPSec --orbitTimeAsc=$pcc_orbitTimeAsc --orbitTimeAscBand=$pcc_orbitTimeAscBand --numBins=$pcc_numBins --mismatchF=$pcc_mismatchF --mismatchA=$pcc_mismatchA --mismatchT=$pcc_mismatchT --mismatchP=$pcc_mismatchP"

## ---------- Run MFDv4 ----------
cmdline="$mfd_code $mfd_CL1";
echo $cmdline
echo -n "Running ${mfd_code} ... "
if ! eval "$cmdline"; then
    echo "FAILED:"
    echo $cmdline
    exit 1
else
    echo "OK."
fi

cmdline="$mfd_code $mfd_CL2";
echo $cmdline
echo -n "Running ${mfd_code} ... "
if ! eval "$cmdline"; then
    echo "FAILED:"
    echo $cmdline
    exit 1
else
    echo "OK."
fi

## ---------- Run PulsarCrossCorr_v2 ----------
cmdline="$pcc_code $pcc_CL"
echo $cmdline
echo -n "Running ${pcc_code} ... "
if ! tmp=`eval $cmdline`; then
    echo "FAILED:"
    echo $cmdline
    exit 1;
else
    echo "OK."
fi

rm -rf ./sfts/
rm -f ./toplist_crosscorr.dat
