const bcrypt = require('bcrypt');
const crypto = require('crypto');

// 1. Mã hóa mật khẩu (Hash Password) trước khi lưu vào bảng users
async function hashPassword(plainPassword) {
    const saltRounds = 10;
    return await bcrypt.hash(plainPassword, saltRounds);
}

// 2. Kiểm tra mật khẩu đăng nhập
async function verifyPassword(plainPassword, hashedPassword) {
    return await bcrypt.compare(plainPassword, hashedPassword);
}

// 3. Tạo mã băm Refresh Token (SHA-256) để lưu vào cột refresh_hash bảng sessions
function generateRefreshHash(refreshToken) {
    return crypto.createHash('sha256').update(refreshToken).digest('hex');
}

// 4. Sinh chuỗi token ngẫu nhiên an toàn
function generateRandomToken() {
    return crypto.randomBytes(32).toString('hex');
}

module.exports = {
    hashPassword,
    verifyPassword,
    generateRefreshHash,
    generateRandomToken
};