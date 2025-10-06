mkdir -p ../frames_kmg
for f in *.png; do
    kmgenc -1 "$f"
    mv "${f%.png}.kmg" ../frames_kmg/
done