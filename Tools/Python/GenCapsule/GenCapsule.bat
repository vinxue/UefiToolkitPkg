set OPENSSL_PATH=C:\OpenSSL-Win32\bin
REM --guid -> PcdSystemFmpCapsuleImageTypeIdGuid
GenerateCapsule.py --encode --guid B4E63FE2-260E-49A2-9EF3-E49D93A57734 --monotonic-count 0x2 --private-key TestSigningPrivateKey.pem -o Capsule.bin SYSTEMFIRMWAREUPDATECARGO.Fv
