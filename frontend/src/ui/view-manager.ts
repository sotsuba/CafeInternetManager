export type ViewState = { view: "dashboard" } | { view: "exclusive"; backendId: number };

const dashboardView = document.getElementById("dashboardView") as HTMLDivElement;
const exclusiveView = document.getElementById("exclusiveView") as HTMLElement;

export function switchView(state: ViewState): void {
    if (state.view === "dashboard") {
        dashboardView.classList.remove("is-hidden");
        exclusiveView.classList.remove("is-active");
    } else {
        dashboardView.classList.add("is-hidden");
        exclusiveView.classList.add("is-active");
    }
}
