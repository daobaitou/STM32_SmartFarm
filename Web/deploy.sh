#!/bin/bash
# SmartFarm Flask Web 部署脚本 (在腾讯云服务器上运行)

set -e

APP_DIR="/opt/smartfarm"
DB_DIR="/data"

echo "=== SmartFarm 部署脚本 ==="

# 1. 安装依赖
echo "[1] 安装系统依赖..."
apt-get update
apt-get install -y python3 python3-pip python3-venv nginx

# 2. 创建应用目录
echo "[2] 创建目录..."
mkdir -p $APP_DIR $DB_DIR

# 3. 复制项目文件（假设当前目录包含Web项目）
echo "[3] 复制项目文件..."
cp -r app.py config.py templates static requirements.txt $APP_DIR/

# 4. Python虚拟环境 + 安装依赖
echo "[4] 安装Python依赖..."
cd $APP_DIR
python3 -m venv venv
./venv/bin/pip install --upgrade pip
./venv/bin/pip install -r requirements.txt

# 5. 初始化数据库
echo "[5] 初始化数据库..."
./venv/bin/python -c "from app import init_db; init_db()"

# 6. systemd服务
echo "[6] 配置systemd服务..."
cp smartfarm.service /etc/systemd/system/
systemctl daemon-reload
systemctl enable smartfarm
systemctl restart smartfarm

# 7. Nginx反向代理
echo "[7] 配置Nginx..."

# 7.1 备份并禁用所有现有网站配置
echo "[7.1] 备份现有Nginx配置..."
BACKUP_DIR="/etc/nginx/backup_$(date +%Y%m%d_%H%M%S)"
mkdir -p $BACKUP_DIR

if [ -d /etc/nginx/sites-enabled ]; then
    # 备份现有配置
    for f in /etc/nginx/sites-enabled/*; do
        [ -f "$f" ] && cp "$f" $BACKUP_DIR/ && echo "  备份: $(basename $f)"
    done
    # 删除所有现有网站配置
    rm -f /etc/nginx/sites-enabled/*
    echo "  已清空 sites-enabled/"
fi

if [ -d /etc/nginx/conf.d ]; then
    for f in /etc/nginx/conf.d/*.conf; do
        [ -f "$f" ] && cp "$f" $BACKUP_DIR/ && echo "  备份: $(basename $f)"
    done
    rm -f /etc/nginx/conf.d/*.conf
    echo "  已清空 conf.d/"
fi

# 7.2 部署smartfarm配置
if [ -d /etc/nginx/sites-available ]; then
    cp nginx.conf /etc/nginx/sites-available/smartfarm
    ln -sf /etc/nginx/sites-available/smartfarm /etc/nginx/sites-enabled/smartfarm
    echo "  已配置 sites-enabled/smartfarm"
else
    cp nginx.conf /etc/nginx/conf.d/smartfarm.conf
    echo "  已配置 conf.d/smartfarm.conf"
fi

# 7.3 测试并重载
echo "[7.3] 测试Nginx配置..."
nginx -t && systemctl reload nginx
echo "  Nginx重载成功"

echo ""
echo "=== 部署完成 ==="
echo "访问: http://124.223.5.91"
echo "状态: systemctl status smartfarm"
echo "原网站备份: $BACKUP_DIR"
echo "恢复原网站: cp $BACKUP_DIR/* /etc/nginx/sites-enabled/ && systemctl reload nginx"