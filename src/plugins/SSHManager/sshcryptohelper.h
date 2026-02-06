/*
    SPDX-FileCopyrightText: 2025 Konsole Contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SSHCRYPTOHELPER_H
#define SSHCRYPTOHELPER_H

#include <QByteArray>
#include <QString>

/**
 * AES-256-GCM encryption helper for the SSHManager plugin.
 *
 * Uses OpenSSL's EVP API with PBKDF2-HMAC-SHA256 key derivation.
 * Encrypted strings are prefixed with "ENC:" for easy detection.
 *
 * Wire format: salt[16] || iv[12] || tag[16] || ciphertext
 */
class SSHCryptoHelper
{
public:
    static constexpr int SALT_SIZE = 16;
    static constexpr int IV_SIZE = 12;
    static constexpr int TAG_SIZE = 16;
    static constexpr int KEY_SIZE = 32; // AES-256
    static constexpr int PBKDF2_ITERATIONS = 100000;
    static constexpr auto ENCRYPTED_PREFIX = "ENC:";

    /**
     * Encrypt a plaintext string with a password.
     * Returns "ENC:base64(salt||iv||tag||ciphertext)" or empty string on failure.
     * Empty plaintext returns empty string (nothing to encrypt).
     */
    static QString encrypt(const QString &plaintext, const QString &password);

    /**
     * Decrypt an "ENC:..." string with a password.
     * Returns the decrypted plaintext, or empty string on failure.
     * If the input is not encrypted (no ENC: prefix), returns it unchanged.
     */
    static QString decrypt(const QString &ciphertext, const QString &password);

    /** Returns true if the string starts with "ENC:" */
    static bool isEncrypted(const QString &value);

    /**
     * Encrypt a raw byte blob with a password.
     * Returns salt||iv||tag||ciphertext, or empty on failure.
     */
    static QByteArray encryptBlob(const QByteArray &data, const QString &password);

    /**
     * Decrypt a raw byte blob (salt||iv||tag||ciphertext) with a password.
     * Returns the decrypted data, or empty on failure.
     */
    static QByteArray decryptBlob(const QByteArray &data, const QString &password);

private:
    static QByteArray deriveKey(const QString &password, const QByteArray &salt);
    static QByteArray randomBytes(int count);
};

#endif // SSHCRYPTOHELPER_H
