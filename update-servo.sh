#!/bin/sh


if [ ! -d mozglue -o ! -d servo ]; then
  echo "Abort: It doesn't look like you're in an incubator repo."
  exit 1
fi

if [ `hg root` != `pwd` ]; then
  echo "Abort: It doesn't look you're in an hg repo, which is required to push merge commits."
  exit 1
fi

echo "Checking to make sure your tree is clean..."
if [ ! -z "$(hg diff)" ]; then
  echo "Abort: It doesn't look like your tree is clean."
  exit 1
fi

echo "invoking |rm -rf servo && rm -rf third_party/rust| in 3 seconds. Press ctrl-c to abort..."
sleep 3
echo "rm -rf servo && rm -rf third_party/rust"
rm -rf servo && rm -rf third_party/rust
mkdir third_party/rust

export SERVO_REV=`git ls-remote https://github.com/servo/servo HEAD | awk '{print $1}'`
echo "Fetching Servo rev $SERVO_REV..."
wget https://github.com/servo/servo/archive/master.zip
echo "Unzipping Servo..."
unzip -q master.zip
mv servo-master servo
rm master.zip
echo "Removing test files..."
rm -rf servo/tests/wpt/css-tests
rm -rf servo/tests/wpt/web-platform-tests

echo "Hiding .cargo/ so that we can re-vendor dependencies..."
mv .cargo .cargo-inactive
echo "Re-vendoring dependencies..."
(
  cd toolkit/library/rust
  cargo update
  cargo vendor ../../../third_party/rust
)
( cd toolkit/library/gtest/rust && cargo update )
echo "Restoring .cargo/..."
mv .cargo-inactive .cargo
echo "Committing..."
hg addremove
hg commit -m "Update servo to $SERVO_REV."
echo "Done!"

