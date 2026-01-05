/**
 * Landing Page - Premium Aurora Design
 * Hero section with animated gradients and feature showcase
 */

import { router } from '../main';

export function LandingPage(): HTMLElement {
  const page = document.createElement('div');
  page.className = 'landing-page';

  page.innerHTML = `
    <!-- Decorative Orbs -->
    <div class="glow-orb glow-orb-purple" style="width: 600px; height: 600px; top: -200px; right: -200px; opacity: 0.15;"></div>
    <div class="glow-orb glow-orb-cyan" style="width: 500px; height: 500px; bottom: -100px; left: -150px; opacity: 0.1;"></div>
    <div class="glow-orb glow-orb-pink" style="width: 400px; height: 400px; top: 40%; left: 60%; opacity: 0.08;"></div>

    <!-- Hero Section -->
    <section class="hero" id="hero">
      <!-- Badge -->
      <div class="animate-fade-in-down" style="animation-delay: 0.1s;">
        <span class="badge badge-gradient mb-6" style="padding: 8px 16px; font-size: 0.875rem;">
          ✨ Phiên bản 2.0 - Aurora Dark
        </span>
      </div>

      <!-- Title -->
      <h1 class="hero-title animate-fade-in-up" style="animation-delay: 0.2s;">
        CafeManager
      </h1>

      <!-- Subtitle -->
      <p class="hero-subtitle animate-fade-in-up" style="animation-delay: 0.3s;">
        Hệ thống quản lý máy tính từ xa toàn diện với thiết kế <strong class="text-gradient">tiên tiến</strong>.
        Giám sát, điều khiển và bảo mật mọi thiết bị trong mạng của bạn.
      </p>

      <!-- CTA Buttons -->
      <div class="flex gap-4 animate-fade-in-up" style="animation-delay: 0.4s;">
        <button class="btn btn-primary btn-lg" id="btn-start">
          <span>🚀</span>
          <span>Bắt Đầu Sử Dụng</span>
        </button>
        <a href="https://github.com" class="btn btn-outline btn-lg" target="_blank">
          <span>📚</span>
          <span>Xem Tài Liệu</span>
        </a>
      </div>

      <!-- Stats Row -->
      <div class="flex gap-8 mt-12 animate-fade-in" style="animation-delay: 0.6s;">
        <div class="text-center">
          <div class="text-3xl font-bold text-gradient-purple">24/7</div>
          <div class="text-sm text-muted">Giám sát</div>
        </div>
        <div class="text-center">
          <div class="text-3xl font-bold text-gradient">< 50ms</div>
          <div class="text-sm text-muted">Độ trễ</div>
        </div>
        <div class="text-center">
          <div class="text-3xl font-bold text-gradient-cyan">100%</div>
          <div class="text-sm text-muted">Bảo mật</div>
        </div>
      </div>

      <!-- Scroll Indicator -->
      <div class="animate-float mt-16" style="animation-delay: 1s;">
        <div style="display: flex; flex-direction: column; align-items: center; gap: 8px; color: var(--text-muted);">
          <span class="text-xs">Cuộn xuống</span>
          <span style="font-size: 1.5rem;">↓</span>
        </div>
      </div>
    </section>

    <!-- Features Section -->
    <section class="container" style="padding: 120px 24px; position: relative; z-index: 1;">
      <div class="text-center mb-12">
        <h2 class="text-4xl font-bold mb-4" style="background: var(--gradient-sunset); -webkit-background-clip: text; -webkit-text-fill-color: transparent;">
          Tính Năng Nổi Bật
        </h2>
        <p class="text-secondary text-lg max-w-2xl mx-auto">
          Mọi công cụ bạn cần để quản lý và giám sát hệ thống máy tính từ xa
        </p>
      </div>

      <div class="features-grid">
        ${createFeatureCard('🖥️', 'Streaming Màn Hình', 'Xem màn hình theo thời gian thực với độ trễ cực thấp. Hỗ trợ ghi hình và chụp ảnh.', 'purple')}
        ${createFeatureCard('📹', 'Webcam Streaming', 'Truy cập webcam từ xa để giám sát an ninh hoặc giao tiếp.', 'cyan')}
        ${createFeatureCard('⌨️', 'Keylogger', 'Ghi lại toàn bộ hoạt động bàn phím với timestamps chi tiết.', 'pink')}
        ${createFeatureCard('📋', 'Quản Lý Tiến Trình', 'Xem và điều khiển các tiến trình đang chạy. Kết thúc ứng dụng từ xa.', 'blue')}
        ${createFeatureCard('🚀', 'Khởi Chạy Ứng Dụng', 'Mở ứng dụng từ xa với giao diện launcher hiện đại.', 'indigo')}
        ${createFeatureCard('📁', 'Quản Lý Tệp', 'Duyệt, tải lên, tải xuống và xóa tệp với giao diện Finder-style.', 'teal')}
        ${createFeatureCard('🖱️', 'Điều Khiển Chuột/Phím', 'Điều khiển hoàn toàn chuột và bàn phím của máy từ xa.', 'violet')}
        ${createFeatureCard('🔐', 'Bảo Mật Cao', 'Kết nối được mã hóa, xác thực hai lớp, và kiểm soát truy cập.', 'success')}
      </div>
    </section>

    <!-- Architecture Section -->
    <section class="container" style="padding: 80px 24px; position: relative; z-index: 1;">
      <div class="glass-card-strong" style="max-width: 900px; margin: 0 auto;">
        <h3 class="text-2xl font-semibold text-center mb-8" style="background: var(--gradient-ocean); -webkit-background-clip: text; -webkit-text-fill-color: transparent;">
          Kiến Trúc Hệ Thống
        </h3>

        <div class="architecture-diagram">
          <div class="arch-node arch-frontend">
            <div class="arch-node-icon">🌐</div>
            <div class="arch-node-label">Frontend</div>
            <div class="arch-node-desc">React/Vite</div>
          </div>

          <div class="arch-connector">
            <div class="arch-connector-line"></div>
            <span class="arch-connector-label">WebSocket</span>
          </div>

          <div class="arch-node arch-gateway">
            <div class="arch-node-icon">🔌</div>
            <div class="arch-node-label">Gateway</div>
            <div class="arch-node-desc">C Server</div>
          </div>

          <div class="arch-connector">
            <div class="arch-connector-line"></div>
            <span class="arch-connector-label">TCP/IP</span>
          </div>

          <div class="arch-node arch-backend">
            <div class="arch-node-icon">💻</div>
            <div class="arch-node-label">Backends</div>
            <div class="arch-node-desc">Win/Linux/Mac</div>
          </div>
        </div>

        <div class="flex justify-center gap-6 mt-8">
          <div class="text-center">
            <div class="text-aurora-cyan text-2xl font-bold">1</div>
            <div class="text-xs text-muted">Frontend kết nối Gateway</div>
          </div>
          <div class="text-center">
            <div class="text-aurora-purple text-2xl font-bold">2</div>
            <div class="text-xs text-muted">Gateway định tuyến lệnh</div>
          </div>
          <div class="text-center">
            <div class="text-aurora-pink text-2xl font-bold">3</div>
            <div class="text-xs text-muted">Backend thực thi & phản hồi</div>
          </div>
        </div>
      </div>
    </section>

    <!-- Team Section -->
    <section class="container" style="padding: 80px 24px 120px; position: relative; z-index: 1;">
      <div class="text-center mb-12">
        <h2 class="text-3xl font-bold mb-4">Đội Ngũ Phát Triển</h2>
        <p class="text-muted">Nhóm 8 - Bộ môn Kỹ thuật Mạng Máy Tính</p>
      </div>

      <div class="team-grid">
        ${createTeamCard('👨‍💻', 'Nguyễn Khánh Linh', 'Leader - Frontend')}
        ${createTeamCard('🧑‍💻', 'Trần Văn A', 'Gateway Developer')}
        ${createTeamCard('👩‍💻', 'Phạm Thị B', 'Backend Developer')}
        ${createTeamCard('🧑‍🔬', 'Lê Văn C', 'Security & Testing')}
      </div>
    </section>

    <!-- Footer -->
    <footer style="padding: 24px; text-align: center; border-top: 1px solid var(--border-subtle); position: relative; z-index: 1;">
      <p class="text-sm text-muted">
        © 2025 CafeManager v2.0 - Aurora Dark Theme
      </p>
    </footer>
  `;

  // Add page-specific styles
  const style = document.createElement('style');
  style.textContent = `
    /* Features Grid */
    .features-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
      gap: var(--space-6);
    }

    .feature-card {
      padding: var(--space-7);
      position: relative;
      overflow: hidden;
      transition: all var(--transition-normal);
    }

    .feature-card::before {
      content: '';
      position: absolute;
      inset: 0;
      background: var(--gradient);
      opacity: 0;
      transition: opacity var(--transition-normal);
    }

    .feature-card:hover {
      transform: translateY(-8px);
      border-color: var(--accent-color);
    }

    .feature-card:hover::before {
      opacity: 0.05;
    }

    .feature-icon {
      font-size: 2.5rem;
      margin-bottom: var(--space-4);
      display: inline-block;
      animation: float 3s ease-in-out infinite;
    }

    .feature-card:nth-child(odd) .feature-icon {
      animation-delay: 0.5s;
    }

    .feature-title {
      font-size: var(--text-lg);
      font-weight: var(--font-semibold);
      margin-bottom: var(--space-2);
      color: var(--text-primary);
    }

    .feature-desc {
      font-size: var(--text-sm);
      color: var(--text-tertiary);
      line-height: var(--leading-relaxed);
    }

    /* Architecture Diagram */
    .architecture-diagram {
      display: flex;
      align-items: center;
      justify-content: center;
      flex-wrap: wrap;
      gap: var(--space-4);
      padding: var(--space-8) 0;
    }

    .arch-node {
      display: flex;
      flex-direction: column;
      align-items: center;
      padding: var(--space-5) var(--space-6);
      background: var(--bg-elevated);
      border-radius: var(--radius-xl);
      border: 1px solid var(--border-subtle);
      min-width: 120px;
      transition: all var(--transition-normal);
    }

    .arch-node:hover {
      transform: translateY(-4px);
      box-shadow: var(--shadow-lg);
    }

    .arch-gateway {
      background: var(--gradient-primary);
      border: none;
      box-shadow: var(--glow-primary);
    }

    .arch-node-icon {
      font-size: 2rem;
      margin-bottom: var(--space-2);
    }

    .arch-node-label {
      font-weight: var(--font-semibold);
      font-size: var(--text-sm);
    }

    .arch-node-desc {
      font-size: var(--text-xs);
      color: var(--text-muted);
      margin-top: var(--space-1);
    }

    .arch-gateway .arch-node-desc {
      color: rgba(255, 255, 255, 0.7);
    }

    .arch-connector {
      display: flex;
      flex-direction: column;
      align-items: center;
      padding: 0 var(--space-4);
    }

    .arch-connector-line {
      width: 60px;
      height: 2px;
      background: var(--gradient-aurora);
      border-radius: var(--radius-full);
    }

    .arch-connector-label {
      font-size: var(--text-2xs);
      color: var(--text-muted);
      margin-top: var(--space-2);
      text-transform: uppercase;
      letter-spacing: 0.1em;
    }

    /* Team Grid */
    .team-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(220px, 1fr));
      gap: var(--space-5);
      max-width: 1000px;
      margin: 0 auto;
    }

    .team-card {
      text-align: center;
      padding: var(--space-6);
      transition: all var(--transition-normal);
    }

    .team-card:hover {
      transform: translateY(-6px);
      border-color: var(--aurora-purple);
      box-shadow: var(--glow-purple);
    }

    .team-avatar {
      font-size: 3rem;
      margin-bottom: var(--space-4);
      display: inline-block;
    }

    .team-name {
      font-weight: var(--font-semibold);
      color: var(--text-primary);
      margin-bottom: var(--space-1);
    }

    .team-role {
      font-size: var(--text-sm);
      color: var(--text-muted);
    }

    /* Color Variables for Cards */
    .feature-card[data-color="purple"] {
      --accent-color: var(--aurora-purple);
      --gradient: linear-gradient(135deg, var(--aurora-purple), var(--aurora-violet));
    }
    .feature-card[data-color="cyan"] {
      --accent-color: var(--aurora-cyan);
      --gradient: linear-gradient(135deg, var(--aurora-cyan), var(--aurora-teal));
    }
    .feature-card[data-color="pink"] {
      --accent-color: var(--aurora-pink);
      --gradient: linear-gradient(135deg, var(--aurora-pink), var(--aurora-fuchsia));
    }
    .feature-card[data-color="blue"] {
      --accent-color: var(--aurora-blue);
      --gradient: linear-gradient(135deg, var(--aurora-blue), var(--aurora-indigo));
    }
    .feature-card[data-color="indigo"] {
      --accent-color: var(--aurora-indigo);
      --gradient: linear-gradient(135deg, var(--aurora-indigo), var(--aurora-purple));
    }
    .feature-card[data-color="teal"] {
      --accent-color: var(--aurora-teal);
      --gradient: linear-gradient(135deg, var(--aurora-teal), var(--aurora-cyan));
    }
    .feature-card[data-color="violet"] {
      --accent-color: var(--aurora-violet);
      --gradient: linear-gradient(135deg, var(--aurora-violet), var(--aurora-purple));
    }
    .feature-card[data-color="success"] {
      --accent-color: var(--accent-success);
      --gradient: linear-gradient(135deg, var(--accent-success), #059669);
    }

    /* Aurora text colors */
    .text-aurora-cyan { color: var(--aurora-cyan); }
    .text-aurora-purple { color: var(--aurora-purple); }
    .text-aurora-pink { color: var(--aurora-pink); }

    .mx-auto { margin-left: auto; margin-right: auto; }
  `;
  page.appendChild(style);

  // Event handlers
  setTimeout(() => {
    const btnStart = page.querySelector('#btn-start');
    btnStart?.addEventListener('click', () => {
      router.navigate('/connect');
    });
  }, 0);

  return page;
}

function createFeatureCard(icon: string, title: string, desc: string, color: string): string {
  return `
    <div class="feature-card glass-card stagger-item" data-color="${color}">
      <div class="feature-icon">${icon}</div>
      <h3 class="feature-title">${title}</h3>
      <p class="feature-desc">${desc}</p>
    </div>
  `;
}

function createTeamCard(avatar: string, name: string, role: string): string {
  return `
    <div class="team-card glass-card stagger-item">
      <div class="team-avatar animate-float">${avatar}</div>
      <div class="team-name">${name}</div>
      <div class="team-role">${role}</div>
    </div>
  `;
}
