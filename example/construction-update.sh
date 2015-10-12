mkdir plots
mkdir plots/construction_update

dorun=$1

runs=$2

echo $runs

if [ $dorun -eq 1 ]
then
  prun -runs $runs -prog "rake-compress-construction.virtual -seq 2,rake-compress-construction.virtual -seq 0,rake-compress-update.virtual -seq 1 -k 999999,rake-compress-update.virtual -seq 0 -k 999999" -n 1000000 -graph binary_tree -fraction 0 -output "plots/construction_update/results.txt"
  for f in 0 0.3 0.6 1
  do
    prun --append -runs $runs -prog "rake-compress-construction.virtual -seq 2,rake-compress-construction.virtual -seq 0,rake-compress-update.virtual -seq 1 -k 999999,rake-compress-update.virtual -seq 0 -k 999999" -n 1000000 -graph random_tree -fraction $f -output "plots/construction_update/results.txt"
  done
fi
cp plots/construction_update/results.txt plots/construction_update/plot.pdf
pplot bar -series prog -y exectime -x graph,fraction -input plots/construction_update/plot.pdf --xtitles-vertical
cp _results/chart-1.pdf plots/construction_update/plot.pdf
