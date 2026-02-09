import { Outlet } from 'react-router-dom';
import Sidebar from './Sidebar';
import StepProgress from './StepProgress';
import { useSidebar } from '../context/SidebarContext';

export default function Layout() {
  const { collapsed } = useSidebar();

  return (
    <div className="flex min-h-screen bg-dark">
      <Sidebar />
      <main
        className={`flex-1 overflow-auto transition-[margin] duration-300 ease-in-out ${
          collapsed ? 'ml-[80px]' : 'ml-[260px]'
        }`}
      >
        <StepProgress />
        <Outlet />
      </main>
    </div>
  );
}
