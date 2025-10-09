from pywebio import start_server
from pywebio.output import put_text

def web_app():
    """
    This function defines the content of the web page.
    """
    put_text("Hello from the standalone web server!")

def main():
    """
    Main function to start the PyWebIO server.
    """
    host_address = '0.0.0.0'
    port_number = 8080 
    
    print(f'Starting standalone web server on http://{host_address}:{port_number}')
    
    # This will block and run the web server until it is closed.
    start_server(web_app, host=host_address, port=port_number)

if __name__ == '__main__':
    main()