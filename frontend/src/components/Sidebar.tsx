import { NavLink, useLocation } from 'react-router-dom';
import { 
  Home, Upload, BarChart3, Play, TrendingUp, 
  Shield, Map, Truck, Users, Download,
  ChevronLeft
} from 'lucide-react';
import { useSidebar } from '../context/SidebarContext';
import { useApp } from '../context/AppContext';

const navigation = [
  { name: 'Dashboard', to: '/', icon: Home, requires: null },
  { name: 'Upload Data', to: '/upload', icon: Upload, requires: null },
  { name: 'Data Insights', to: '/insights', icon: BarChart3, requires: 'data' },
  { name: 'Processing', to: '/processing', icon: Play, requires: 'data' },
  { name: 'Results', to: '/results', icon: TrendingUp, requires: 'results' },
  { name: 'Constraints', to: '/constraints', icon: Shield, requires: 'results' },
  { name: 'Map', to: '/routes', icon: Map, requires: 'results' },
  { name: 'Fleet', to: '/fleet', icon: Truck, requires: 'results' },
  { name: 'Employees', to: '/employees', icon: Users, requires: 'results' },
  { name: 'Export', to: '/export', icon: Download, requires: 'results' },
];

export default function Sidebar() {
  const { collapsed, toggle } = useSidebar();
  const { currentResult, optimizationStatus } = useApp();
  const location = useLocation(); // Re-render on route change

  const hasData = !!sessionStorage.getItem('uploadedData');
  // Only unlock results sections when optimization has fully completed
  const hasResults = sessionStorage.getItem('optimizationComplete') === 'true' && (optimizationStatus === 'completed' || !!currentResult);

  const isDisabled = (item: typeof navigation[0]) => {
    if (!item.requires) return false;
    if (item.requires === 'data') return !hasData;
    if (item.requires === 'results') return !hasResults;
    return false;
  };

  return (
    <aside
      className={`fixed top-0 left-0 z-40 h-screen p-3 flex flex-col gap-2 transition-[width] duration-300 ease-in-out ${
        collapsed ? 'w-[80px]' : 'w-[260px]'
      }`}
    >
      {/* Logo Block — separate floating card */}
      <div className="relative bg-dark-800/80 backdrop-blur-xl rounded-2xl border border-gray/10 px-3 py-3 flex items-center overflow-hidden shadow-float">
        <div
          className={`flex-1 min-w-0 overflow-hidden transition-[opacity] duration-200 ${
            collapsed ? 'opacity-0 w-0' : 'opacity-100'
          }`}
        >
          <h1 className="text-xl font-display font-extrabold text-white tracking-wide leading-tight whitespace-nowrap pl-1">
            VELORA
          </h1>
        </div>

        <button
          onClick={toggle}
          className="flex-shrink-0 w-8 h-8 rounded-full bg-dark-600 flex items-center justify-center text-gray/60 hover:text-white hover:bg-dark-500 transition-colors ml-auto"
        >
          <ChevronLeft
            className={`w-4 h-4 transition-transform duration-300 ${collapsed ? 'rotate-180' : ''}`}
          />
        </button>
      </div>

      {/* Navigation Block — main floating card */}
      <div className="flex-1 flex flex-col bg-dark-800/80 backdrop-blur-xl rounded-2xl border border-gray/10 overflow-hidden shadow-float">
        <nav className="flex-1 px-2 py-3 space-y-0.5 overflow-y-auto scrollbar-thin">
          {navigation.map((item) => {
            const disabled = isDisabled(item);
            
            if (disabled) {
              return (
                <div
                  key={item.name}
                  title={collapsed ? item.name : `${item.name} (locked)`}
                  className="flex items-center gap-3 rounded-xl text-sm overflow-hidden px-3 py-2.5 text-white/20 cursor-not-allowed"
                >
                  <item.icon className="w-[18px] h-[18px] flex-shrink-0" />
                  <span
                    className={`whitespace-nowrap transition-[opacity] duration-200 ${
                      collapsed ? 'opacity-0 w-0' : 'opacity-100'
                    }`}
                  >
                    {item.name}
                  </span>
                </div>
              );
            }

            return (
              <NavLink
                key={item.name}
                to={item.to}
                title={item.name}
                className={({ isActive }) =>
                  `flex items-center gap-3 rounded-xl transition-all duration-200 text-sm overflow-hidden px-3 py-2.5 ${
                    isActive
                      ? 'bg-white/90 text-dark-800 font-semibold'
                      : 'text-white/80 hover:text-white hover:bg-white/5'
                  }`
                }
              >
                <item.icon className="w-[18px] h-[18px] flex-shrink-0" />
                <span
                  className={`whitespace-nowrap transition-[opacity] duration-200 ${
                    collapsed ? 'opacity-0 w-0' : 'opacity-100'
                  }`}
                >
                  {item.name}
                </span>
              </NavLink>
            );
          })}
        </nav>

        {/* Footer */}
        <div
          className={`px-4 py-3 border-t border-gray/10 overflow-hidden transition-[opacity] duration-200 ${
            collapsed ? 'opacity-0 h-0 py-0 border-0' : 'opacity-100'
          }`}
        >
          <div className="text-[10px] text-gray/40 text-center whitespace-nowrap">
            © 2026 Velora Optimization
          </div>
        </div>
      </div>
    </aside>
  );
}

