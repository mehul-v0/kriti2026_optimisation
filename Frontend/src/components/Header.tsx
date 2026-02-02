import { Zap } from 'lucide-react';

export function Header() {
  return (
    <header className="bg-gradient-to-r from-slate-900 to-slate-800 text-white shadow-lg">
      <div className="container mx-auto px-6 py-4">
        <div className="flex items-center justify-between">
          <div className="flex items-center space-x-3">
            <div className="bg-emerald-500 p-2 rounded-lg">
              <Zap className="w-6 h-6" />
            </div>
            <div>
              <h1 className="text-2xl font-bold tracking-tight">VELORA</h1>
              <p className="text-xs text-slate-300">Driven by Possibility</p>
            </div>
          </div>
          <div className="text-right">
            <p className="text-sm font-medium">Corporate Mobility Optimization</p>
            <p className="text-xs text-slate-400">Real-time Route Intelligence</p>
          </div>
        </div>
      </div>
    </header>
  );
}
