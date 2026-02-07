import { NavLink } from 'react-router-dom';
import { 
  Home, Upload, BarChart3, Play, TrendingUp, 
  Shield, Map, Truck, Users, DollarSign, Download
} from 'lucide-react';

const navigation = [
  { name: 'Dashboard', to: '/', icon: Home },
  { name: 'Upload Data', to: '/upload', icon: Upload },
  { name: 'Data Insights', to: '/insights', icon: BarChart3 },
  { name: 'Processing', to: '/processing', icon: Play },
  { name: 'Results', to: '/results', icon: TrendingUp },
  { name: 'Constraints', to: '/constraints', icon: Shield },
  { name: 'Routes', to: '/routes', icon: Map },
  { name: 'Fleet', to: '/fleet', icon: Truck },
  { name: 'Employees', to: '/employees', icon: Users },
  { name: 'Costs', to: '/costs', icon: DollarSign },
  { name: 'Export', to: '/export', icon: Download },
];

export default function Sidebar() {
  return (
    <aside className="w-64 bg-dark-800 border-r border-gray/20 flex flex-col">
      <div className="p-6 border-b border-gray/20">
        <h1 className="text-2xl font-bold text-gradient font-display">VELORA</h1>
        <p className="text-xs text-gray mt-1">Driven by Possibility</p>
      </div>
      
      <nav className="flex-1 p-4 space-y-1 overflow-y-auto scrollbar-thin">
        {navigation.map((item) => (
          <NavLink
            key={item.name}
            to={item.to}
            className={({ isActive }) =>
              `flex items-center gap-3 px-4 py-3 rounded-lg transition-all ${
                isActive
                  ? 'bg-primary text-dark font-semibold'
                  : 'text-gray hover:text-white hover:bg-dark-700'
              }`
            }
          >
            <item.icon className="w-5 h-5" />
            <span>{item.name}</span>
          </NavLink>
        ))}
      </nav>

      <div className="p-4 border-t border-gray/20">
        <div className="text-xs text-gray text-center">
          © 2026 Velora Optimization
        </div>
      </div>
    </aside>
  );
}
