class VideoManagementSystem {
    constructor() {
        // Auto-detect server URL based on current location
        this.baseUrl = window.location.origin;
        this.websocket = null;
        this.streams = new Map();
        this.activeStreams = 0;
        
        this.initializeElements();
        this.setupEventListeners();
        this.initializeWebSocket();
        this.loadStreams();
    }
    
    initializeElements() {
        this.elements = {
            streamsGrid: document.getElementById('streamsGrid'),
            startAllBtn: document.getElementById('startAllBtn'),
            stopAllBtn: document.getElementById('stopAllBtn'),
            connectionStatus: document.getElementById('connectionStatus'),
            connectionText: document.getElementById('connectionText'),
            activeStreams: document.getElementById('activeStreams'),
            streamModal: document.getElementById('streamModal'),
            modalTitle: document.getElementById('modalTitle'),
            modalStreamId: document.getElementById('modalStreamId'),
            modalStatus: document.getElementById('modalStatus'),
            modalResolution: document.getElementById('modalResolution'),
            modalFrameRate: document.getElementById('modalFrameRate'),
            modalStreamUrl: document.getElementById('modalStreamUrl'),
            modalStartBtn: document.getElementById('modalStartBtn'),
            modalStopBtn: document.getElementById('modalStopBtn'),
            close: document.querySelector('.close')
        };
    }
    
    setupEventListeners() {
        this.elements.startAllBtn.addEventListener('click', () => this.startAllStreams());
        this.elements.stopAllBtn.addEventListener('click', () => this.stopAllStreams());
        this.elements.modalStartBtn.addEventListener('click', () => this.startCurrentStream());
        this.elements.modalStopBtn.addEventListener('click', () => this.stopCurrentStream());
        this.elements.close.addEventListener('click', () => this.closeModal());
        
        // Close modal when clicking outside
        window.addEventListener('click', (event) => {
            if (event.target === this.elements.streamModal) {
                this.closeModal();
            }
        });
    }
    
    initializeWebSocket() {
        try {
            // Use WebSocket URL based on current location
            const wsUrl = this.baseUrl.replace('http://', 'ws://').replace('https://', 'wss://');
            this.websocket = new WebSocket(wsUrl);
            
            this.websocket.onopen = () => {
                this.updateConnectionStatus(true);
                console.log('WebSocket connected');
            };
            
            this.websocket.onmessage = (event) => {
                try {
                    const data = JSON.parse(event.data);
                    this.handleWebSocketMessage(data);
                } catch (error) {
                    console.error('Error parsing WebSocket message:', error);
                }
            };
            
            this.websocket.onclose = () => {
                this.updateConnectionStatus(false);
                console.log('WebSocket disconnected');
                // Attempt to reconnect after 3 seconds
                setTimeout(() => this.initializeWebSocket(), 3000);
            };
            
            this.websocket.onerror = (error) => {
                console.error('WebSocket error:', error);
                this.updateConnectionStatus(false);
            };
        } catch (error) {
            console.error('Failed to initialize WebSocket:', error);
            this.updateConnectionStatus(false);
        }
    }
    
    updateConnectionStatus(connected) {
        if (connected) {
            this.elements.connectionStatus.className = 'status-dot online';
            this.elements.connectionText.textContent = 'Connected';
        } else {
            this.elements.connectionStatus.className = 'status-dot offline';
            this.elements.connectionText.textContent = 'Disconnected';
        }
        
        // Update footer with current server info
        const serverInfo = document.getElementById('serverInfo');
        if (serverInfo) {
            serverInfo.textContent = this.baseUrl.replace('http://', '').replace('https://', '');
        }
    }
    
    handleWebSocketMessage(data) {
        if (data.type === 'stream_update') {
            this.updateStreamStatus(data.streamId, data.active);
        }
    }
    
    async loadStreams() {
        try {
            const response = await fetch(`${this.baseUrl}/api/streams`);
            const data = await response.json();
            
            this.elements.streamsGrid.innerHTML = '';
            this.streams.clear();
            this.activeStreams = 0;
            
            // Create 8 stream cards
            for (let i = 0; i < 8; i++) {
                const streamData = data.streams.find(s => s.id === i);
                const isActive = streamData ? streamData.active : false;
                
                if (isActive) this.activeStreams++;
                
                this.streams.set(i, { id: i, active: isActive });
                this.createStreamCard(i, isActive);
            }
            
            this.updateActiveStreamsCount();
        } catch (error) {
            console.error('Error loading streams:', error);
            this.showError('Failed to load streams');
        }
    }
    
    createStreamCard(streamId, isActive) {
        const card = document.createElement('div');
        card.className = 'stream-card';
        card.innerHTML = `
            <div class="stream-header">
                <h3 class="stream-title">Stream ${streamId + 1}</h3>
                <span class="status-badge ${isActive ? 'active' : 'inactive'}">
                    ${isActive ? 'Active' : 'Inactive'}
                </span>
            </div>
            <div class="stream-preview ${isActive ? 'active' : 'inactive'}" onclick="vms.openStream(${streamId})">
                <i class="fas fa-${isActive ? 'play-circle' : 'stop-circle'}"></i>
                ${isActive ? '<div class="stream-overlay">Click to view stream</div>' : '<div class="stream-overlay">Stream inactive</div>'}
            </div>
            <div class="stream-info">
                <div class="info-item">
                    <span class="info-label">Resolution:</span>
                    <span class="info-value">1920x1080</span>
                </div>
                <div class="info-item">
                    <span class="info-label">Frame Rate:</span>
                    <span class="info-value">30 FPS</span>
                </div>
                <div class="info-item">
                    <span class="info-label">Port:</span>
                    <span class="info-value">${8081 + streamId}</span>
                </div>
                <div class="info-item">
                    <span class="info-label">Codec:</span>
                    <span class="info-value">H.264</span>
                </div>
            </div>
            <div class="stream-actions">
                <button class="btn btn-${isActive ? 'danger' : 'success'}" 
                        onclick="vms.${isActive ? 'stop' : 'start'}Stream(${streamId})">
                    <i class="fas fa-${isActive ? 'stop' : 'play'}"></i>
                    ${isActive ? 'Stop Stream' : 'Start Stream'}
                </button>
                <button class="btn btn-secondary" onclick="vms.showStreamDetails(${streamId})">
                    <i class="fas fa-cog"></i>
                    Settings
                </button>
            </div>
        `;
        
        this.elements.streamsGrid.appendChild(card);
    }
    
    async startStream(streamId) {
        try {
            this.setButtonLoading(streamId, 'start', true);
            
            const response = await fetch(`${this.baseUrl}/api/stream/${streamId}/start`, {
                method: 'POST'
            });
            
            const data = await response.json();
            
            if (data.success) {
                this.updateStreamStatus(streamId, true);
                this.showSuccess(`Stream ${streamId + 1} started successfully`);
            } else {
                this.showError(`Failed to start stream ${streamId + 1}`);
            }
        } catch (error) {
            console.error('Error starting stream:', error);
            this.showError(`Failed to start stream ${streamId + 1}`);
        } finally {
            this.setButtonLoading(streamId, 'start', false);
        }
    }
    
    async stopStream(streamId) {
        try {
            this.setButtonLoading(streamId, 'stop', true);
            
            const response = await fetch(`${this.baseUrl}/api/stream/${streamId}/stop`, {
                method: 'POST'
            });
            
            const data = await response.json();
            
            if (data.success) {
                this.updateStreamStatus(streamId, false);
                this.showSuccess(`Stream ${streamId + 1} stopped successfully`);
            } else {
                this.showError(`Failed to stop stream ${streamId + 1}`);
            }
        } catch (error) {
            console.error('Error stopping stream:', error);
            this.showError(`Failed to stop stream ${streamId + 1}`);
        } finally {
            this.setButtonLoading(streamId, 'stop', false);
        }
    }
    
    async startAllStreams() {
        this.elements.startAllBtn.innerHTML = '<div class="loading"></div> Starting...';
        this.elements.startAllBtn.disabled = true;
        
        try {
            const promises = [];
            for (let i = 0; i < 8; i++) {
                promises.push(this.startStream(i));
            }
            
            await Promise.all(promises);
            this.showSuccess('All streams started successfully');
        } catch (error) {
            this.showError('Failed to start some streams');
        } finally {
            this.elements.startAllBtn.innerHTML = '<i class="fas fa-play"></i> Start All';
            this.elements.startAllBtn.disabled = false;
        }
    }
    
    async stopAllStreams() {
        this.elements.stopAllBtn.innerHTML = '<div class="loading"></div> Stopping...';
        this.elements.stopAllBtn.disabled = true;
        
        try {
            const promises = [];
            for (let i = 0; i < 8; i++) {
                promises.push(this.stopStream(i));
            }
            
            await Promise.all(promises);
            this.showSuccess('All streams stopped successfully');
        } catch (error) {
            this.showError('Failed to stop some streams');
        } finally {
            this.elements.stopAllBtn.innerHTML = '<i class="fas fa-stop"></i> Stop All';
            this.elements.stopAllBtn.disabled = false;
        }
    }
    
    updateStreamStatus(streamId, isActive) {
        const stream = this.streams.get(streamId);
        if (stream) {
            stream.active = isActive;
        }
        
        // Update the stream card
        const cards = this.elements.streamsGrid.children;
        if (cards[streamId]) {
            const card = cards[streamId];
            const statusBadge = card.querySelector('.status-badge');
            const preview = card.querySelector('.stream-preview');
            const button = card.querySelector('.stream-actions .btn:first-child');
            const icon = button.querySelector('i');
            
            if (isActive) {
                statusBadge.className = 'status-badge active';
                statusBadge.textContent = 'Active';
                preview.className = 'stream-preview active';
                button.className = 'btn btn-danger';
                button.innerHTML = '<i class="fas fa-stop"></i> Stop';
                icon.className = 'fas fa-play';
            } else {
                statusBadge.className = 'status-badge inactive';
                statusBadge.textContent = 'Inactive';
                preview.className = 'stream-preview inactive';
                button.className = 'btn btn-success';
                button.innerHTML = '<i class="fas fa-play"></i> Start';
                icon.className = 'fas fa-stop';
            }
        }
        
        // Update active streams count
        this.activeStreams = Array.from(this.streams.values()).filter(s => s.active).length;
        this.updateActiveStreamsCount();
    }
    
    updateActiveStreamsCount() {
        this.elements.activeStreams.textContent = this.activeStreams;
    }
    
    setButtonLoading(streamId, action, loading) {
        const cards = this.elements.streamsGrid.children;
        if (cards[streamId]) {
            const button = cards[streamId].querySelector(`.stream-actions .btn:first-child`);
            if (loading) {
                button.innerHTML = '<div class="loading"></div>';
                button.disabled = true;
            } else {
                const isActive = this.streams.get(streamId).active;
                button.innerHTML = `<i class="fas fa-${isActive ? 'stop' : 'play'}"></i> ${isActive ? 'Stop' : 'Start'}`;
                button.disabled = false;
            }
        }
    }
    
    showStreamDetails(streamId) {
        const stream = this.streams.get(streamId);
        if (!stream) return;
        
        this.elements.modalTitle.textContent = `Stream ${streamId + 1} Details`;
        this.elements.modalStreamId.textContent = streamId;
        this.elements.modalStatus.textContent = stream.active ? 'Active' : 'Inactive';
        this.elements.modalStatus.className = `status-badge ${stream.active ? 'active' : 'inactive'}`;
        this.elements.modalResolution.textContent = '1920x1080';
        this.elements.modalFrameRate.textContent = '30 FPS';
        this.elements.modalStreamUrl.value = `udp://127.0.0.1:${8081 + streamId}`;
        
        // Update modal buttons
        this.elements.modalStartBtn.style.display = stream.active ? 'none' : 'inline-flex';
        this.elements.modalStopBtn.style.display = stream.active ? 'inline-flex' : 'none';
        
        this.elements.streamModal.style.display = 'block';
        this.currentStreamId = streamId;
    }
    
    closeModal() {
        this.elements.streamModal.style.display = 'none';
        this.currentStreamId = null;
    }
    
    startCurrentStream() {
        if (this.currentStreamId !== null) {
            this.startStream(this.currentStreamId);
            this.closeModal();
        }
    }
    
    stopCurrentStream() {
        if (this.currentStreamId !== null) {
            this.stopStream(this.currentStreamId);
            this.closeModal();
        }
    }
    
    showSuccess(message) {
        this.showNotification(message, 'success');
    }
    
    showError(message) {
        this.showNotification(message, 'error');
    }
    
    openStream(streamId) {
        const stream = this.streams.get(streamId);
        if (stream && stream.active) {
            // Open stream in new window
            const streamUrl = `${this.baseUrl}/stream/${streamId}`;
            window.open(streamUrl, `stream_${streamId}`, 'width=800,height=600,scrollbars=yes,resizable=yes');
        } else {
            this.showError(`Stream ${streamId + 1} is not active`);
        }
    }
    
    showNotification(message, type) {
        // Create notification element
        const notification = document.createElement('div');
        notification.className = `notification ${type}`;
        notification.innerHTML = `
            <i class="fas fa-${type === 'success' ? 'check-circle' : 'exclamation-circle'}"></i>
            <span>${message}</span>
        `;
        
        // Add styles
        notification.style.cssText = `
            position: fixed;
            top: 20px;
            right: 20px;
            background: ${type === 'success' ? '#27ae60' : '#e74c3c'};
            color: white;
            padding: 1rem 1.5rem;
            border-radius: 8px;
            box-shadow: 0 4px 12px rgba(0, 0, 0, 0.2);
            z-index: 1001;
            display: flex;
            align-items: center;
            gap: 0.5rem;
            animation: slideIn 0.3s ease;
        `;
        
        document.body.appendChild(notification);
        
        // Remove after 3 seconds
        setTimeout(() => {
            notification.style.animation = 'slideOut 0.3s ease';
            setTimeout(() => {
                if (notification.parentNode) {
                    notification.parentNode.removeChild(notification);
                }
            }, 300);
        }, 3000);
    }
}

// Add CSS animations for notifications
const style = document.createElement('style');
style.textContent = `
    @keyframes slideIn {
        from {
            transform: translateX(100%);
            opacity: 0;
        }
        to {
            transform: translateX(0);
            opacity: 1;
        }
    }
    
    @keyframes slideOut {
        from {
            transform: translateX(0);
            opacity: 1;
        }
        to {
            transform: translateX(100%);
            opacity: 0;
        }
    }
`;
document.head.appendChild(style);

// Initialize the application
let vms;
document.addEventListener('DOMContentLoaded', () => {
    vms = new VideoManagementSystem();
});
