/**
 * CafeManager v2 - Aurora Dark Dashboard
 * Main Application Entry Point
 */

import { DashboardPage } from './pages/DashboardPage';
import { GatewayPage } from './pages/GatewayPage';
import { LandingPage } from './pages/LandingPage';
import { Router } from './services/Router';

// Initialize Application
const app = document.getElementById('app')!;

// Simple Router
export const router = new Router(app, {
  '/': LandingPage,
  '/connect': GatewayPage,
  '/dashboard': DashboardPage,
});

// Start the app
router.navigate(window.location.hash.slice(1) || '/');

// Handle browser navigation
window.addEventListener('hashchange', () => {
  router.navigate(window.location.hash.slice(1) || '/');
});

console.log('🚀 CafeManager v2 initialized');
