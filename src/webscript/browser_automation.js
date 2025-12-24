// browser_automation.js - 浏览器自动化辅助函数
// 提供类似 Playwright 的 API，供 C++ 通过 ExecuteScript 调用

// 禁用 Windows Hello / Passkey（立即执行）
(function () {
    // 禁用 PublicKeyCredential.isUserVerifyingPlatformAuthenticatorAvailable
    const pkc = window.PublicKeyCredential;
    if (pkc) {
        try {
            Object.defineProperty(pkc, 'isUserVerifyingPlatformAuthenticatorAvailable', {
                configurable: true,
                enumerable: false,
                value: () => Promise.resolve(false)
            });
        } catch (e) {
            try {
                pkc.isUserVerifyingPlatformAuthenticatorAvailable = () => Promise.resolve(false);
            } catch (_) { }
        }
    }

    // 拦截 navigator.credentials.create（用于创建 Passkey）
    try {
        const nav = window.navigator;
        if (nav && nav.credentials && typeof nav.credentials.create === 'function') {
            const originalCreate = nav.credentials.create.bind(nav.credentials);
            nav.credentials.create = function (options) {
                if (options && options.publicKey) {
                    console.log('[MSLogin] 已拦截 Passkey 保存请求');
                    return Promise.reject(new DOMException('User cancelled', 'NotAllowedError'));
                }
                return originalCreate(options);
            };
        }
    } catch (e) {
        console.error('[MSLogin] 拦截 credentials.create 失败:', e);
    }

    console.log('[MSLogin] Windows Hello / Passkey 已禁用');
})();

window.__automation = {
    // 错误码定义
    ERR_SUCCESS: 0,           // 成功
    ERR_EXCEPTION: 1,         // 处理过程中发生异常
    ERR_NO_HANDLER: 2,        // 没有对应处理函数
    ERR_NOT_FOUND: 3,         // 元素未找到
    ERR_TIMEOUT: 4,           // 超时

    // 登录凭据（由 C++ 设置）
    credentials: {
        username: '',
        password: ''
    },

    // 设置登录凭据
    setCredentials: function (username, password) {
        this.credentials.username = username;
        this.credentials.password = password;
        return JSON.stringify({ errcode: this.ERR_SUCCESS });
    },

    // 页面操作处理器映射
    actionHandlers: {
        '登录': function (auto) {
            auto.type('[id="usernameEntry"]', auto.credentials.username);
            auto.clickButton('下一步');
        },
        '输入你的密码': function (auto) {
            auto.type('[id="passwordEntry"]', auto.credentials.password);
            auto.clickButton('下一步');
        },
        '获取用于登录的代码': function (auto) {
            auto.clickButton('其他登录方法');
        },
        '使用另一种方式登录': function (auto) {
            auto.clickButton('使用密码');
        },
        '验证你的电子邮件': function (auto) {
            auto.clickButton('使用密码');
        },
        '使用人脸、指纹或 PIN 更快地登录': function (auto) {
            auto.clickButton('暂时跳过');
        },
        '正在尝试登录到 Game Pass 吗?': function (auto) {
            auto.clickButton('继续');
        },
        '无法创建通行密钥': function (auto) {
            auto.clickButton('取消');
        }
    },

    // 执行指定页面的 action（C++ 调用此方法，传入页面标题）
    // 返回: { errcode: 0|1|2, message?: string }
    doAction: function (title) {
        try {
            // 查找对应的处理器
            const handler = this.actionHandlers[title];
            if (!handler) {
                return JSON.stringify({
                    errcode: this.ERR_NO_HANDLER,
                    message: 'No handler for: ' + title
                });
            }

            // 执行处理器
            handler(this);

            return JSON.stringify({
                errcode: this.ERR_SUCCESS
            });
        } catch (e) {
            return JSON.stringify({
                errcode: this.ERR_EXCEPTION,
                message: e.message
            });
        }
    },

    // 查找元素（返回是否存在）
    querySelector: function (selector) {
        const element = document.querySelector(selector);
        if (element) {
            return JSON.stringify({ errcode: this.ERR_SUCCESS });
        }
        return JSON.stringify({ errcode: this.ERR_NOT_FOUND, message: 'Element not found: ' + selector });
    },

    // 点击元素
    click: function (selector) {
        const element = document.querySelector(selector);
        if (!element) {
            return JSON.stringify({ errcode: this.ERR_NOT_FOUND, message: 'Element not found: ' + selector });
        }

        try {
            element.click();
            return JSON.stringify({ errcode: this.ERR_SUCCESS });
        } catch (e) {
            return JSON.stringify({ errcode: this.ERR_EXCEPTION, message: e.message });
        }
    },

    // 通过文本查找并点击按钮
    clickButton: function (text) {
        const buttons = document.querySelectorAll('button, [role="button"]');
        for (const button of buttons) {
            if (button.textContent.trim() === text || button.value === text) {
                button.click();
                return JSON.stringify({ errcode: this.ERR_SUCCESS });
            }
        }
        return JSON.stringify({ errcode: this.ERR_NOT_FOUND, message: 'Button not found: ' + text });
    },

    // 填写输入框（使用原生 setter 触发 React）
    type: function (selector, value) {
        const element = document.querySelector(selector);
        if (!element) {
            return JSON.stringify({ errcode: this.ERR_NOT_FOUND, message: 'Element not found: ' + selector });
        }

        try {
            element.focus();

            // 获取原生 setter（兼容 React）
            const nativeInputValueSetter = Object.getOwnPropertyDescriptor(
                window.HTMLInputElement.prototype,
                'value'
            ).set;

            // 清空
            nativeInputValueSetter.call(element, '');

            // 逐字符输入并触发事件
            for (let i = 0; i < value.length; i++) {
                nativeInputValueSetter.call(element, element.value + value[i]);
                element.dispatchEvent(new Event('input', { bubbles: true }));
            }

            element.dispatchEvent(new Event('change', { bubbles: true }));
            element.blur();

            return JSON.stringify({ errcode: this.ERR_SUCCESS, value: element.value });
        } catch (e) {
            return JSON.stringify({ errcode: this.ERR_EXCEPTION, message: e.message });
        }
    },

    // 获取元素文本内容
    getText: function (selector) {
        const element = document.querySelector(selector);
        if (!element) {
            return JSON.stringify({ errcode: this.ERR_NOT_FOUND, message: 'Element not found: ' + selector });
        }
        return JSON.stringify({ errcode: this.ERR_SUCCESS, text: element.textContent.trim() });
    },

    // 获取输入框的值
    getValue: function (selector) {
        const element = document.querySelector(selector);
        if (!element) {
            return JSON.stringify({ errcode: this.ERR_NOT_FOUND, message: 'Element not found: ' + selector });
        }
        return JSON.stringify({ errcode: this.ERR_SUCCESS, value: element.value });
    },

    // 等待元素出现（轮询版本，返回是否成功）
    waitForSelector: function (selector, timeoutMs) {
        const self = this;
        timeoutMs = timeoutMs || 5000;
        const startTime = Date.now();

        return new Promise((resolve) => {
            const check = () => {
                if (document.querySelector(selector)) {
                    resolve(JSON.stringify({ errcode: self.ERR_SUCCESS }));
                } else if (Date.now() - startTime > timeoutMs) {
                    resolve(JSON.stringify({ errcode: self.ERR_TIMEOUT, message: 'Timeout waiting for: ' + selector }));
                } else {
                    setTimeout(check, 100);
                }
            };
            check();
        });
    },

    // 获取页面标题（Microsoft 登录页通过 data-testid="title" 显示当前步骤）
    getPageTitle: function () {
        var titleElement = document.querySelector('[data-testid="title"]');
        if (titleElement) {
            return JSON.stringify({ errcode: this.ERR_SUCCESS, title: titleElement.textContent.trim() });
        }

        titleElement = document.querySelector('div#appConfirmPageTitle.text-title')
        if (titleElement) {
            return JSON.stringify({ errcode: this.ERR_SUCCESS, title: titleElement.textContent.trim() });
        }

        return JSON.stringify({ errcode: this.ERR_NOT_FOUND, message: 'Title element not found' });
    },

    // 列出所有可见按钮的文本
    listButtons: function () {
        const buttons = document.querySelectorAll('button, [role="button"]');
        const buttonTexts = [];
        for (const button of buttons) {
            const text = button.textContent.trim();
            if (text) {
                buttonTexts.push(text);
            }
        }
        return JSON.stringify({ errcode: this.ERR_SUCCESS, buttons: buttonTexts });
    }
};

// 标记脚本已加载
window.__automation.ready = true;
