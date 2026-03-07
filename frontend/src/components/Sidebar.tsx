import { NavLink, useLocation } from 'react-router-dom';
import { useApp } from '../context/AppContext';

const navigation = [
  { name: 'Dashboard', to: '/', icon: 'pie_chart', requires: null },
  { name: 'Upload', to: '/upload', icon: 'upload', requires: null },
  { name: 'Insights', to: '/insights', icon: 'trending_up', requires: 'data' },
  { name: 'Processing', to: '/processing', icon: 'sync', requires: 'data' },
  { name: 'Results', to: '/results', icon: 'check_circle', requires: 'results' },
  { name: 'Constraints', to: '/constraints', icon: 'shield', requires: 'results' },
  { name: 'Map', to: '/routes', icon: 'map', requires: 'results' },
  { name: 'Fleet', to: '/fleet', icon: 'local_shipping', requires: 'results' },
  { name: 'Employees', to: '/employees', icon: 'groups', requires: 'results' },
  { name: 'Export', to: '/export', icon: 'download', requires: 'results' },
];

export default function Sidebar() {
  const { currentResult, optimizationStatus } = useApp();
  useLocation(); // Re-render on route change

  const hasData = !!sessionStorage.getItem('uploadedData');
  const hasResults = sessionStorage.getItem('optimizationComplete') === 'true' && (optimizationStatus === 'completed' || !!currentResult);

  const isDisabled = (item: typeof navigation[0]) => {
    if (!item.requires) return false;
    if (item.requires === 'data') return !hasData;
    if (item.requires === 'results') return !hasResults;
    return false;
  };

  return (
    <aside className="w-64 flex-shrink-0 border-r border-border-dark bg-background-light dark:bg-[#0a1511] flex flex-col justify-between">
      <div className="flex flex-col gap-4 p-4">
        {/* Logo */}
        <div className="flex items-center gap-3 mb-6">
          <div className="w-10 h-10 rounded-full bg-primary/20 flex items-center justify-center">
            <span className="material-symbols-outlined text-primary text-xl">hub</span>
          </div>
          <div className="flex flex-col">
            <h1 className="text-slate-900 dark:text-white text-base font-bold leading-normal tracking-wide">VELORA</h1>
            <p className="text-slate-500 dark:text-slate-400 text-xs font-medium uppercase tracking-wider">VRP Platform</p>
          </div>
        </div>

        {/* Navigation */}
        <nav className="flex flex-col gap-1">
          {navigation.map((item) => {
            const disabled = isDisabled(item);

            if (disabled) {
              return (
                <div
                  key={item.name}
                  title={`${item.name} (locked)`}
                  className="flex items-center gap-3 px-3 py-2.5 rounded-lg text-slate-600/30 dark:text-slate-500/30 cursor-not-allowed"
                >
                  <span className="material-symbols-outlined text-[22px]">{item.icon}</span>
                  <p className="text-sm font-medium leading-normal">{item.name}</p>
                </div>
              );
            }

            return (
              <NavLink
                key={item.name}
                to={item.to}
                className={({ isActive }) =>
                  `flex items-center gap-3 px-3 py-2.5 rounded-lg transition-colors ${
                    isActive
                      ? 'bg-primary/10 text-primary'
                      : 'hover:bg-surface-dark-hover text-slate-600 dark:text-slate-300'
                  }`
                }
              >
                <span className="material-symbols-outlined text-[22px]">{item.icon}</span>
                <p className={`text-sm leading-normal`}>{item.name}</p>
              </NavLink>
            );
          })}
        </nav>
      </div>

      {/* Footer / User info */}
      <div className="p-4 border-t border-border-dark">
        <div className="flex items-center gap-3 px-3 py-2">
          <div className="w-8 h-8 rounded-full bg-primary/20 flex items-center justify-center">
            <span className="material-symbols-outlined text-primary text-base">person</span>
          </div>
          <div className="flex flex-col">
            <p className="text-sm font-medium leading-normal">Fleet Manager</p>
            <p className="text-xs text-slate-500 dark:text-slate-400">Logistics Dept</p>
          </div>
        </div>
      </div>
    </aside>
  );
}

