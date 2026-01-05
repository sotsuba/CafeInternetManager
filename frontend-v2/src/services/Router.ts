/**
 * Simple Hash-based Router
 */

type PageComponent = () => HTMLElement;

interface Routes {
  [path: string]: PageComponent;
}

export class Router {
  private container: HTMLElement;
  private routes: Routes;
  private currentPath: string = '';

  constructor(container: HTMLElement, routes: Routes) {
    this.container = container;
    this.routes = routes;
  }

  navigate(path: string): void {
    if (this.currentPath === path) return;

    this.currentPath = path;
    window.location.hash = path;

    // Clear current content
    this.container.innerHTML = '';

    // Get page component
    const PageComponent = this.routes[path] || this.routes['/'];

    if (PageComponent) {
      const page = PageComponent();
      this.container.appendChild(page);
    }
  }

  getCurrentPath(): string {
    return this.currentPath;
  }
}
