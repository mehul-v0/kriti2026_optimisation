import { NavLink, Link, useLocation } from 'react-router-dom';
import { useApp } from '../context/AppContext';

const primaryNav = [
  { name: 'Dashboard', to: '/', icon: 'pie_chart', requires: null },
  { name: 'Upload', to: '/upload', icon: 'upload', requires: null },
  { name: 'Insights', to: '/insights', icon: 'trending_up', requires: 'data' },
];

const analyticsNav = [
  { name: 'Results', to: '/results', icon: 'check_circle', requires: 'results' },
  { name: 'Constraints', to: '/constraints', icon: 'shield', requires: 'results' },
  { name: 'Map', to: '/routes', icon: 'map', requires: 'results' },
  { name: 'Fleet', to: '/fleet', icon: 'local_shipping', requires: 'results' },
  { name: 'Employees', to: '/employees', icon: 'groups', requires: 'results' },
  { name: 'Export', to: '/export', icon: 'download', requires: 'results' },
];

const allNav = [...primaryNav, ...analyticsNav];

export default function Topbar() {
  const { currentResult, optimizationStatus } = useApp();
  const location = useLocation();

  const hasData = !!sessionStorage.getItem('uploadedData');
  const hasResults =
    sessionStorage.getItem('optimizationComplete') === 'true' &&
    (optimizationStatus === 'completed' || !!currentResult);

  const isDisabled = (item: (typeof allNav)[0]) => {
    if (!item.requires) return false;
    if (item.requires === 'data') return !hasData;
    if (item.requires === 'results') return !hasResults;
    return false;
  };

  const analyticsRoutes = analyticsNav.map((n) => n.to);
  const onAnalyticsPage = analyticsRoutes.some(
    (r) => location.pathname === r || location.pathname.startsWith(r + '/')
  );

  const visibleNav = onAnalyticsPage ? analyticsNav : primaryNav;

  return (
    <header className="h-12 flex items-center justify-between whitespace-nowrap border-b border-white/5 bg-panel-dark/50 px-6 sticky top-0 z-50">
      {/* Left: brand + nav */}
      <div className="flex items-center gap-6">
        {/* Brand */}
        <NavLink to="/" className="flex items-center gap-3">
          <div className="size-8 bg-primary flex items-center justify-center glow-amber">
            <span className="material-symbols-outlined text-background-dark font-bold text-lg">terminal</span>
          </div>
          <h2 className="text-white/90 text-sm font-mono font-bold tracking-tighter uppercase">
            Velora
          </h2>
        </NavLink>

        {/* Separator */}
        <div className="w-px h-5 bg-white/10" />

        {/* Nav links */}
        <nav className="hidden md:flex items-center gap-0.5">
          {onAnalyticsPage && (
            <NavLink
              to="/"
              className="flex items-center gap-1.5 mr-2 px-3 py-1.5 text-[11px] font-label font-bold uppercase tracking-widest text-white/40 hover:text-primary transition-colors"
            >
              <span className="material-symbols-outlined text-sm">arrow_back</span>
              Dashboard
            </NavLink>
          )}

          {visibleNav.map((item) => {
            const disabled = isDisabled(item);

            if (disabled) {
              return (
                <div
                  key={item.name}
                  title={`${item.name} (locked)`}
                  className="flex items-center gap-1.5 px-3 py-1.5 text-[11px] font-label font-bold uppercase tracking-widest text-white/15 cursor-not-allowed select-none"
                >
                  <span className="material-symbols-outlined text-sm">{item.icon}</span>
                  {item.name}
                </div>
              );
            }

            return (
              <NavLink
                key={item.name}
                to={item.to}
                end={item.to === '/'}
                className={({ isActive }) =>
                  `flex items-center gap-1.5 px-3 py-1.5 text-[11px] font-label font-bold uppercase tracking-widest transition-colors ${
                    isActive
                      ? 'text-primary border-b-2 border-primary'
                      : 'text-white/50 hover:text-primary'
                  }`
                }
              >
                <span className="material-symbols-outlined text-sm">{item.icon}</span>
                {item.name}
              </NavLink>
            );
          })}

          {!onAnalyticsPage && (
            <NavLink
              to="/results"
              className={() => {
                const disabled = !hasResults;
                return `flex items-center gap-1.5 px-3 py-1.5 text-[11px] font-label font-bold uppercase tracking-widest transition-colors ${
                  disabled
                    ? 'text-white/15 cursor-not-allowed pointer-events-none'
                    : 'text-white/50 hover:text-primary'
                }`;
              }}
              onClick={(e) => {
                if (!hasResults) e.preventDefault();
              }}
            >
              <span className="material-symbols-outlined text-sm">analytics</span>
              Analytics
            </NavLink>
          )}
        </nav>
      </div>

      {/* Right: system info + actions */}
      <div className="flex items-center gap-4">
        {/* New Optimization — only on analytics pages */}
        {onAnalyticsPage && (
          <Link
            to="/upload"
            className="flex items-center gap-1.5 px-3 py-1 bg-primary text-background-dark text-[11px] font-label font-bold uppercase tracking-widest hover:bg-primary/90 transition-colors glow-amber"
          >
            <span className="material-symbols-outlined text-sm">add</span>
            New Optimization
          </Link>
        )}

        {/* System status */}
        <div className="hidden lg:flex items-center gap-2">
          <span className="size-2 bg-primary rounded-full animate-pulse" />
          <span className="text-[9px] font-mono text-white/30 uppercase tracking-wider">System Active</span>
        </div>

        {/* Notification */}
        <button className="p-1.5 hover:bg-white/5 text-white/40 hover:text-white transition-colors relative">
          <span className="material-symbols-outlined text-lg">notifications</span>
        </button>

        {/* Settings */}
        <button className="p-1.5 hover:bg-white/5 text-white/40 hover:text-white transition-colors">
          <span className="material-symbols-outlined text-lg">settings</span>
        </button>

        {/* Avatar */}
        <div className="size-7 border border-white/10 flex items-center justify-center bg-primary/10">
          <span className="material-symbols-outlined text-primary text-sm">person</span>
        </div>
      </div>
    </header>
  );
}
