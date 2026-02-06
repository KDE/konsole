/*
    SPDX-FileCopyrightText: 2025 Konsole Contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sshcryptohelper.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

QByteArray SSHCryptoHelper::randomBytes(int count)
{
    QByteArray buf(count, Qt::Uninitialized);
    if (RAND_bytes(reinterpret_cast<unsigned char *>(buf.data()), count) != 1) {
        return {};
    }
    return buf;
}

QByteArray SSHCryptoHelper::deriveKey(const QString &password, const QByteArray &salt)
{
    const QByteArray passUtf8 = password.toUtf8();
    QByteArray key(KEY_SIZE, Qt::Uninitialized);

    if (PKCS5_PBKDF2_HMAC(passUtf8.constData(),
                           passUtf8.size(),
                           reinterpret_cast<const unsigned char *>(salt.constData()),
                           salt.size(),
                           PBKDF2_ITERATIONS,
                           EVP_sha256(),
                           KEY_SIZE,
                           reinterpret_cast<unsigned char *>(key.data()))
        != 1) {
        return {};
    }
    return key;
}

bool SSHCryptoHelper::isEncrypted(const QString &value)
{
    return value.startsWith(QLatin1String(ENCRYPTED_PREFIX));
}

QByteArray SSHCryptoHelper::encryptBlob(const QByteArray &data, const QString &password)
{
    if (data.isEmpty() || password.isEmpty()) {
        return {};
    }

    const QByteArray salt = randomBytes(SALT_SIZE);
    const QByteArray iv = randomBytes(IV_SIZE);
    if (salt.isEmpty() || iv.isEmpty()) {
        return {};
    }

    const QByteArray key = deriveKey(password, salt);
    if (key.isEmpty()) {
        return {};
    }

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return {};
    }

    QByteArray result;

    auto cleanup = [&ctx]() {
        EVP_CIPHER_CTX_free(ctx);
    };

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        cleanup();
        return {};
    }

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_SIZE, nullptr) != 1) {
        cleanup();
        return {};
    }

    if (EVP_EncryptInit_ex(ctx,
                           nullptr,
                           nullptr,
                           reinterpret_cast<const unsigned char *>(key.constData()),
                           reinterpret_cast<const unsigned char *>(iv.constData()))
        != 1) {
        cleanup();
        return {};
    }

    QByteArray ciphertext(data.size() + EVP_MAX_BLOCK_LENGTH, Qt::Uninitialized);
    int outLen = 0;

    if (EVP_EncryptUpdate(ctx,
                          reinterpret_cast<unsigned char *>(ciphertext.data()),
                          &outLen,
                          reinterpret_cast<const unsigned char *>(data.constData()),
                          data.size())
        != 1) {
        cleanup();
        return {};
    }

    int totalLen = outLen;
    if (EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char *>(ciphertext.data()) + totalLen, &outLen) != 1) {
        cleanup();
        return {};
    }
    totalLen += outLen;
    ciphertext.resize(totalLen);

    QByteArray tag(TAG_SIZE, Qt::Uninitialized);
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_SIZE, tag.data()) != 1) {
        cleanup();
        return {};
    }

    cleanup();

    // Wire format: salt || iv || tag || ciphertext
    result.reserve(SALT_SIZE + IV_SIZE + TAG_SIZE + ciphertext.size());
    result.append(salt);
    result.append(iv);
    result.append(tag);
    result.append(ciphertext);

    return result;
}

QByteArray SSHCryptoHelper::decryptBlob(const QByteArray &data, const QString &password)
{
    const int headerSize = SALT_SIZE + IV_SIZE + TAG_SIZE;
    if (data.size() < headerSize || password.isEmpty()) {
        return {};
    }

    const QByteArray salt = data.mid(0, SALT_SIZE);
    const QByteArray iv = data.mid(SALT_SIZE, IV_SIZE);
    QByteArray tag = data.mid(SALT_SIZE + IV_SIZE, TAG_SIZE);
    const QByteArray ciphertext = data.mid(headerSize);

    const QByteArray key = deriveKey(password, salt);
    if (key.isEmpty()) {
        return {};
    }

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return {};
    }

    auto cleanup = [&ctx]() {
        EVP_CIPHER_CTX_free(ctx);
    };

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        cleanup();
        return {};
    }

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_SIZE, nullptr) != 1) {
        cleanup();
        return {};
    }

    if (EVP_DecryptInit_ex(ctx,
                           nullptr,
                           nullptr,
                           reinterpret_cast<const unsigned char *>(key.constData()),
                           reinterpret_cast<const unsigned char *>(iv.constData()))
        != 1) {
        cleanup();
        return {};
    }

    QByteArray plaintext(ciphertext.size(), Qt::Uninitialized);
    int outLen = 0;

    if (EVP_DecryptUpdate(ctx,
                          reinterpret_cast<unsigned char *>(plaintext.data()),
                          &outLen,
                          reinterpret_cast<const unsigned char *>(ciphertext.constData()),
                          ciphertext.size())
        != 1) {
        cleanup();
        return {};
    }

    int totalLen = outLen;

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_SIZE, tag.data()) != 1) {
        cleanup();
        return {};
    }

    if (EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char *>(plaintext.data()) + totalLen, &outLen) != 1) {
        // Authentication failed — wrong password or tampered data
        cleanup();
        return {};
    }
    totalLen += outLen;
    plaintext.resize(totalLen);

    cleanup();
    return plaintext;
}

QString SSHCryptoHelper::encrypt(const QString &plaintext, const QString &password)
{
    if (plaintext.isEmpty()) {
        return {};
    }

    const QByteArray blob = encryptBlob(plaintext.toUtf8(), password);
    if (blob.isEmpty()) {
        return {};
    }

    return QLatin1String(ENCRYPTED_PREFIX) + QString::fromLatin1(blob.toBase64());
}

QString SSHCryptoHelper::decrypt(const QString &ciphertext, const QString &password)
{
    if (!isEncrypted(ciphertext)) {
        return ciphertext;
    }

    const QString base64Part = ciphertext.mid(static_cast<int>(qstrlen(ENCRYPTED_PREFIX)));
    const QByteArray blob = QByteArray::fromBase64(base64Part.toLatin1());

    const QByteArray plaintext = decryptBlob(blob, password);
    if (plaintext.isEmpty()) {
        return {};
    }

    return QString::fromUtf8(plaintext);
}
